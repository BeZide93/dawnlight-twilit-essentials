#include "collection_internal.hpp"

#include "d/d_meter2.h"
#include "d/d_meter2_draw.h"

// The library is one translation unit.
#include "collection_common.cpp"
#include "tex_replacements.cpp"
#include "custom_equip.cpp"
#include "collection_layout.cpp"
#include "collection_page.cpp"
#include "collection_screen.cpp"
#include "collection_nav.cpp"
#include "collection_equip.cpp"

// Services the library uses itself, under its own names: a mod that imports the same
// services as svc_resource / svc_host does not collide with them.
IMPORT_OPTIONAL_SERVICE(ResourceService, cl_svc_resource);
IMPORT_OPTIONAL_SERVICE(HostService, cl_svc_host);

const ResourceService* cl_resource_service() { return cl_svc_resource; }
const HostService* cl_host_service() { return cl_svc_host; }

// ---------------------------------------------------------------------------
// Policies
// ---------------------------------------------------------------------------

static bool (*s_unequipPolicy)() = nullptr;
static bool (*s_keepOrdonShieldPolicy)() = nullptr;

void collectionlib_set_unequip_policy(bool (*fn)()) { s_unequipPolicy = fn; }
void collectionlib_set_keep_ordon_shield_policy(bool (*fn)()) { s_keepOrdonShieldPolicy = fn; }

bool cl_unequip_enabled() { return s_unequipPolicy != nullptr && s_unequipPolicy(); }
bool cl_keep_ordon_shield_enabled() { return s_keepOrdonShieldPolicy != nullptr && s_keepOrdonShieldPolicy(); }

// ---------------------------------------------------------------------------
// B button icon of a custom sword
// ---------------------------------------------------------------------------

DEFINE_HOOK(&dMeter2Draw_c::changeTextureItemB, CollectionLibItemBTextureHook);

static void cl_item_b_texture_post(ModContext*, void* args, void*, void*) {
    const int id = custom_equip_active_id(CE_SWORD);
    if (id < 0) return;
    ResTIMG* icon = custom_equip_icon(id);
    if (icon == nullptr) return;

    dMeter2Draw_c* draw = args ? mods::arg<dMeter2Draw_c*>(args, 0) : nullptr;
    if (draw == nullptr) return;

    if (draw->mpItemB != nullptr && draw->mpItemB->getPanePtr() != nullptr) {
        static_cast<J2DPicture*>(draw->mpItemB->getPanePtr())->changeTexture(icon, 0);
    }
    if (draw->mpItemBPane != nullptr) {
        draw->mpItemBPane->hide();
    }
}

static void refresh_item_b_texture() {
    dMeter2_c* meter = g_meter2_info.getMeterClass();
    dMeter2Draw_c* draw = meter != nullptr ? meter->getMeterDrawPtr() : nullptr;
    if (draw != nullptr) draw->changeTextureItemB(dComIfGs_getSelectEquipSword());
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

static void ensure_collection_heap_capacity() {
    static bool s_done = false;
    if (s_done) return;

    JKRHeap* sysHeap = JKRHeap::getSystemHeap();
    JKRHeap* rootHeap = JKRHeap::getRootHeap();
    if (sysHeap == nullptr || rootHeap == nullptr) {
        return;
    }

    if (sysHeap->getFreeSize() < 4 * 1024 * 1024) {
        u32 targetSize = 32 * 1024 * 1024;
        u32 rootFree = rootHeap->getFreeSize();
        if (rootFree < targetSize + 2 * 1024 * 1024) {
            targetSize = (rootFree > 4 * 1024 * 1024) ? (rootFree - 2 * 1024 * 1024) : 0;
        }

        if (targetSize >= 4 * 1024 * 1024) {
            JKRExpHeap* newSysHeap = JKRExpHeap::create(targetSize, rootHeap, false);
            if (newSysHeap != nullptr) {
                newSysHeap->setName("ExpandedSysHeap");
                JKRHeap::setSystemHeap(newSysHeap);
                s_done = true;
            }
        }
    }
}

ModResult collectionlib_init(const HookService* hook_svc, const LogService* log_svc,
                             const SaveService* save_svc, ModContext* mod_ctx, ModError*) {
    ensure_collection_heap_capacity();
    g_modCtx = mod_ctx;
    g_saveSvc = save_svc;
    g_logSvc = log_svc;

    collectionlib_run_slot_registration();
    custom_equip_restore_from_save();

    if (hook_svc != nullptr) {
        screen_install_hooks(hook_svc);
        nav_install_hooks(hook_svc);
        equip_install_hooks(hook_svc);
        custom_equip_init_hooks(hook_svc, g_saveSvc);
        CL_HOOK_POST(CollectionLibItemBTextureHook, cl_item_b_texture_post);
    }
    return MOD_OK;
}

void collectionlib_update() {
    collection_page_update();
    custom_equip_update();
}

void collectionlib_shutdown() {
    collection_page_teardown();
    screen_shutdown();
    custom_equip_shutdown();
    // The meter must not keep showing an icon that is freed next.
    refresh_item_b_texture();
    cl_free_icons();
    s_currentCollect2D = nullptr;
}

// ---------------------------------------------------------------------------
// Mod-link compat
//
// Older SDK headers declare J3DTexture::initGXTexObj out-of-line on PC, but the game binary
// does not export it to mods; the model code needs it. The body mirrors the in-tree one.
// Newer versions export it as loadGXTexObj (CMakeLists.txt tells them apart).
// ---------------------------------------------------------------------------

#if CL_DEFINE_INIT_GX_TEX_OBJ
#include <dolphin/gx.h>
#include "JSystem/J3DGraphBase/J3DTexture.h"

void J3DTexture::initGXTexObj(u16 idx) {
    J3D_ASSERT_RANGE(29, idx < mNum);
    ResTIMG* timg = getResTIMG(idx);

    GXTlutObj& tlutObj = mpTlutObj[idx];
    TGXTexObj& texObj = mpTexObj[idx];

    if (!timg->indexTexture) {
        GXInitTexObj(&texObj, mpImgDataPtr[idx], timg->width, timg->height,
                     (GXTexFmt)timg->format, (GXTexWrapMode)timg->wrapS, (GXTexWrapMode)timg->wrapT,
                     timg->mipmapEnabled);
    } else {
        GXInitTexObjCI(&texObj, mpImgDataPtr[idx], timg->width, timg->height,
                       (GXCITexFmt)timg->format, (GXTexWrapMode)timg->wrapS,
                       (GXTexWrapMode)timg->wrapT, timg->mipmapEnabled, GX_TLUT0);
        GXInitTlutObj(&tlutObj, mpTlutDataPtr[idx], (GXTlutFmt)timg->colorFormat,
                      timg->numColors);
    }

    const f32 kLODClampScale = 1.0f / 8.0f;
    const f32 kLODBiasScale = 1.0f / 100.0f;
    GXInitTexObjLOD(&texObj, (GXTexFilter)timg->minFilter, (GXTexFilter)timg->magFilter,
                    timg->minLOD * kLODClampScale, timg->maxLOD * kLODClampScale,
                    timg->LODBias * kLODBiasScale, timg->biasClamp, timg->doEdgeLOD,
                    (GXAnisotropy)timg->maxAnisotropy);
}
#endif

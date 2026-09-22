#include "boss_rush_save.hpp"

#include "mods/svc/host.h"
#include "mods/svc/resource.h"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/actor/d_a_alink.h"

#include <collection_lib/collection_lib.hpp>

#include <cstdio>
#include <cstring>

extern const HostService* svc_host;
extern const ResourceService* get_resource_service();

namespace {

constexpr u32 kPresetMagic   = 0x42525356;
constexpr u32 kPresetVersion = 1;
constexpr const char* kResPresetPath = "boss_rush/boss_rush_save.bin";
constexpr const char* kExportFileName = "boss_rush_save.bin";

struct PresetHeader {
    u32 magic;
    u32 version;
    u32 payloadSize;
};
static_assert(sizeof(PresetHeader) == 12, "keep the header packed");

ModContext* s_modCtx = nullptr;


dSv_save_c s_presetSave;
bool s_presetAvailable = false;

bool accept_preset_image(const void* data, size_t size, const char* source) {
    if (size < sizeof(PresetHeader) + sizeof(dSv_save_c)) {
        return false;
    }
    const PresetHeader* hdr = static_cast<const PresetHeader*>(data);
    if (hdr->magic != kPresetMagic) {
        return false;
    }
    if (hdr->version != kPresetVersion) {
        return false;
    }
    if (hdr->payloadSize != sizeof(dSv_save_c)) {
        return false;
    }

    std::memcpy(&s_presetSave, static_cast<const u8*>(data) + sizeof(PresetHeader),
                sizeof(dSv_save_c));
    s_presetAvailable = true;
    return true;
}

void load_preset() {
    s_presetAvailable = false;

    if (svc_host != nullptr && s_modCtx != nullptr) {
        const char* dir = nullptr;
        if (svc_host->data_dir(s_modCtx, &dir) == MOD_OK && dir != nullptr) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s/%s", dir, kExportFileName);
            std::FILE* f = std::fopen(path, "rb");
            if (f != nullptr) {
                static u8 s_fileBuf[sizeof(PresetHeader) + sizeof(dSv_save_c)];
                const size_t n = std::fread(s_fileBuf, 1, sizeof(s_fileBuf), f);
                std::fclose(f);
                if (accept_preset_image(s_fileBuf, n, "data_dir")) {
                    return;
                }
            }
        }
    }

    const ResourceService* res_svc = get_resource_service();
    if (res_svc != nullptr && s_modCtx != nullptr) {
        ResourceBuffer buf = RESOURCE_BUFFER_INIT;
        if (res_svc->load(s_modCtx, kResPresetPath, &buf) == MOD_OK) {
            accept_preset_image(buf.data, buf.size, "mod bundle res/");
            res_svc->free(s_modCtx, &buf);
        }
    }
}

}

ModResult init_boss_rush_save(const LogService*, ModContext* mod_ctx) {
    s_modCtx = mod_ctx;
    load_preset();
    return MOD_OK;
}

void shutdown_boss_rush_save() {
    s_presetAvailable = false;
    s_modCtx = nullptr;
}

bool boss_rush_save_preset_available() {
    return s_presetAvailable;
}

bool boss_rush_save_apply_preset(bool i_refreshLink) {
    if (!s_presetAvailable) {
        return false;
    }

    dSv_save_c& live = g_dComIfG_gameInfo.info.getSavedata();
    const u8 prevSword = dComIfGs_getSelectEquipSword();
    const u8 prevShield = dComIfGs_getSelectEquipShield();
    const u8 prevTunic = dComIfGs_getSelectEquipClothes();
    live = s_presetSave;
    dComIfGs_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
    dComIfGs_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
    dComIfGs_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);

    for (int i = 0; i < 4; ++i) {
        dComIfGp_setSelectItem(i);
    }
    dComIfGp_setSelectEquipSword(dItemNo_MASTER_SWORD_e);
    dComIfGp_setSelectEquipShield(dItemNo_HYLIA_SHIELD_e);
    dComIfGp_setSelectEquipClothes(dItemNo_WEAR_KOKIRI_e);

    if (i_refreshLink && (prevSword != dItemNo_MASTER_SWORD_e ||
                          prevShield != dItemNo_HYLIA_SHIELD_e ||
                          prevTunic != dItemNo_WEAR_KOKIRI_e)) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) {
            link->setSelectEquipItem(FALSE);
            link->setClothesChange(0);
        }
    }

    custom_equip_set_suppressed(true);

    return true;
}

void boss_rush_save_apply_equips_to_savedata() {
    const u8 sword = dItemNo_MASTER_SWORD_e;
    const u8 shield = dItemNo_HYLIA_SHIELD_e;
    const u8 tunic = dItemNo_WEAR_KOKIRI_e;

    dComIfGs_setSelectEquipSword(sword);
    dComIfGs_setSelectEquipShield(shield);
    dComIfGs_setSelectEquipClothes(tunic);
    dComIfGp_setSelectEquipSword(sword);
    dComIfGp_setSelectEquipShield(shield);
    dComIfGp_setSelectEquipClothes(tunic);
}

bool boss_rush_save_export_current() {
    if (svc_host == nullptr || s_modCtx == nullptr) {
        return false;
    }

    const char* dir = nullptr;
    if (svc_host->data_dir(s_modCtx, &dir) != MOD_OK || dir == nullptr) {
        return false;
    }

    char path[512];
    std::snprintf(path, sizeof(path), "%s/%s", dir, kExportFileName);

    const dSv_save_c& live = g_dComIfG_gameInfo.info.getSavedata();

    dSv_save_c exportSave = live;
    dSv_player_status_a_c& status = exportSave.getPlayer().getPlayerStatusA();
    for (int btn = 0; btn < 4; ++btn) {
        status.setMixItemIndex(btn, 0xFF);
        status.setSelectItemIndex(btn, 0xFF);
    }

    PresetHeader hdr;
    hdr.magic = kPresetMagic;
    hdr.version = kPresetVersion;
    hdr.payloadSize = sizeof(dSv_save_c);

    std::FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(&hdr, sizeof(hdr), 1, f) == 1 &&
                    std::fwrite(&exportSave, sizeof(dSv_save_c), 1, f) == 1;
    if (std::fclose(f) != 0) {
        return false;
    }
    if (!ok) {
        return false;
    }

    return true;
}

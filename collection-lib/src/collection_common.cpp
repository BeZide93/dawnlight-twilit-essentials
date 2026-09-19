#include "collection_lib/collection_common.hpp"

ModContext* g_modCtx = nullptr;
const SaveService* g_saveSvc = nullptr;
const LogService* g_logSvc = nullptr;

J2DScreen* s_cachedScreen = nullptr;
J2DScreen* s_capturedScreen = nullptr;
dMenu_Collect2D_c* s_currentCollect2D = nullptr;
bool s_needReloadCollect = false;

// Blank-layout mode (see collection_common.hpp).
static bool s_vanillaLayoutHidden = false;
void cl_set_vanilla_layout_hidden(bool hidden) { s_vanillaLayoutHidden = hidden; }
bool cl_vanilla_layout_hidden() { return s_vanillaLayoutHidden; }

// Removed cells (via collectionlib_remove_slot / cl_remove_cell).
static CollectionSlot s_removedCells[16];
static int s_removedCellCount = 0;

void cl_remove_cell(u8 row, u8 item) {
    for (int i = 0; i < s_removedCellCount; i++) {
        if (s_removedCells[i].row == row && s_removedCells[i].item == item) return;
    }
    if (s_removedCellCount < static_cast<int>(sizeof(s_removedCells) / sizeof(s_removedCells[0]))) {
        s_removedCells[s_removedCellCount++] = CollectionSlot{row, item};
    }
}

bool cl_cell_removed(u8 row, u8 item) {
    for (int i = 0; i < s_removedCellCount; i++) {
        if (s_removedCells[i].row == row && s_removedCells[i].item == item) return true;
    }
    return false;
}

// Item-2/item-3 column swap (sword row 1 / shield row 2 only, index row-1).
static bool s_item23Swapped[2] = { false, false };
void cl_set_item23_swapped(u8 row, bool swapped) {
    if (row < 1 || row > 2) return;
    s_item23Swapped[row - 1] = swapped;
}
bool cl_item23_swapped(u8 row) {
    return (row >= 1 && row <= 2) && s_item23Swapped[row - 1];
}

void cl_clear_removed_cell(u8 row, u8 item) {
    for (int i = 0; i < s_removedCellCount; i++) {
        if (s_removedCells[i].row == row && s_removedCells[i].item == item) {
            for (int j = i; j < s_removedCellCount - 1; j++) s_removedCells[j] = s_removedCells[j + 1];
            s_removedCellCount--;
            return;
        }
    }
}

int cl_removed_cell_count() { return s_removedCellCount; }
CollectionSlot cl_removed_cell_at(int i) { return s_removedCells[i]; }

void log_collect_info(const char* fmt, ...) {
    if (g_logSvc == nullptr || g_modCtx == nullptr) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_logSvc->info(g_modCtx, buf);
}

// Connectors (tunagi)
J2DPicture* s_picTunagiKen2 = nullptr;
J2DPicture* s_picTunagiTate2 = nullptr;
J2DPicture* s_picTunagiFuku3 = nullptr;

// Dynamic connectors between adjacent custom slots (up to 6 pairs supported).
J2DPicture* s_customConnectors[6]         = {};
int         s_customConnectorCount        = 0;
J2DPane*    s_customConnectorParent[3]    = {};   // [row-1]: parent pane for row 1/2/3
J2DPicture* s_customConnectorTemplate[3]  = {};   // [row-1]: tunagi01/03/06 source pane


// Sized for every addSlot()-built pane: item1/2/3 overrides across all three
// rows (up to 9) plus auto-placed extras (up to 4 more per row) can add up
// fast - slot_registry_add() drops anything past the end SILENTLY (the pane
// still gets built, just never positioned/shown by layout_managed_slots,
// which only walks this array - the symptom is a "phantom" icon stuck at its
// raw construction position instead of its intended grid slot). Keep this
// comfortably above whatever the consumer currently registers.
static SlotSpec s_slotRegistry[32];
static int      s_slotRegistryCount = 0;

// Last texture actually pushed to each slot's icon picture via changeTexture(),
// indexed the same as s_slotRegistry. layout_managed_slots() runs every frame
// and used to call changeTexture(s->texOverride) unconditionally each time,
// even though texOverride is normally the same pointer for a slot's whole
// screen-build lifetime - that reinitializes the GPU texture object every
// single frame for no reason (visible as constant initTexObj log spam) and
// wastes GPU upload work. Tracked separately from SlotSpec (rather than a
// field on it) so callers holding a const SlotSpec* don't need a const_cast.
static const ResTIMG* s_slotAppliedTex[32] = {};

void slot_registry_clear() {
    s_slotRegistryCount = 0;
    for (auto& t : s_slotAppliedTex) t = nullptr;
}

void slot_registry_add(const SlotSpec& s) {
    if (s_slotRegistryCount < static_cast<int>(sizeof(s_slotRegistry) / sizeof(s_slotRegistry[0]))) {
        s_slotRegistry[s_slotRegistryCount++] = s;
    }
}

const ResTIMG* slot_applied_tex(int i) {
    return (i >= 0 && i < static_cast<int>(sizeof(s_slotAppliedTex) / sizeof(s_slotAppliedTex[0]))) ? s_slotAppliedTex[i] : nullptr;
}

void slot_set_applied_tex(int i, const ResTIMG* tex) {
    if (i >= 0 && i < static_cast<int>(sizeof(s_slotAppliedTex) / sizeof(s_slotAppliedTex[0]))) {
        s_slotAppliedTex[i] = tex;
    }
}

SlotCell grid_cell(u8 row, u8 item) {
    // Natural cell: rows 1..3 -> y 0..2, items 1..4 -> x 3..6.
    if (row >= 1 && row <= 3 && item >= 1 && item <= 4) {
        return SlotCell{ static_cast<u8>(2 + item), static_cast<u8>(row - 1) };
    }
    // Item 5 (custom expansion for each row):
    // row 1, item 5 -> { 2, 0 }
    // row 2, item 5 -> { 2, 1 }
    // row 3, item 5 -> { 2, 2 }
    if (row >= 1 && row <= 3 && item == 5) {
        return SlotCell{ 2, static_cast<u8>(row - 1) };
    }
    // Item 6:
    if (row >= 1 && row <= 3 && item == 6) {
        return SlotCell{ 1, static_cast<u8>(row - 1) };
    }
    // Item 7: the last free hidden column (x=0). Beyond this, the row is full -
    // return "no cell" rather than silently colliding every further item onto x=0.
    if (row >= 1 && row <= 3 && item == 7) {
        return SlotCell{ 0, static_cast<u8>(row - 1) };
    }
    return SlotCell{};
}

int slot_count() { return s_slotRegistryCount; }

const SlotSpec* slot_get(int i) {
    return (i >= 0 && i < s_slotRegistryCount) ? &s_slotRegistry[i] : nullptr;
}

const SlotSpec* slot_at(u8 x, u8 y) {
    for (int i = 0; i < s_slotRegistryCount; i++) {
        if (s_slotRegistry[i].x == x && s_slotRegistry[i].y == y) return &s_slotRegistry[i];
    }
    return nullptr;
}

SlotCell slot_nav_target(u8 x, u8 y, int dir) {
    // 0=left 1=right 2=up 3=down; the reverse of each is dir^1. nav targets are
    // human {row,item} - resolve them the same way addSlot resolves a slot.
    for (int i = 0; i < s_slotRegistryCount; i++) {
        const SlotSpec& s = s_slotRegistry[i];
        if (!s.autoLayout.on) continue;
        const GridPos nbr[4] = { s.autoLayout.navLeft, s.autoLayout.navRight,
                                 s.autoLayout.navUp,   s.autoLayout.navDown };
        // Forward: standing on this slot, press `dir` -> its declared target.
        if (s.x == x && s.y == y && grid_pos_set(nbr[dir])) {
            return grid_cell(nbr[dir].row, nbr[dir].item);
        }
        // Reverse: standing on the slot's `dir^1` neighbour, press `dir` -> this slot.
        const GridPos& rev = nbr[dir ^ 1];
        if (grid_pos_set(rev)) {
            SlotCell rc = grid_cell(rev.row, rev.item);
            if (rc.x == x && rc.y == y) {
                // Horizontal navigation (e.g. Magic Armor -> Ordon Hero, Hylian Shield -> Reinforced Shield)
                if (dir == 0 || dir == 1) return SlotCell{ s.x, s.y };

                // Vertical navigation between custom slots (e.g. DEMOdd <-> Ordon Hero)
                if (slot_at(x, y) != nullptr) return SlotCell{ s.x, s.y };

                // Vertical navigation from vanilla slot that has no vanilla neighbour in this direction
                // (e.g. Magic Armor at row 3 item 4 pressing UP to Reinforced Shield at row 2 item 4)
                if (dir == 2 && rev.item > 3) return SlotCell{ s.x, s.y };
            }
        }
    }
    return SlotCell{};
}

const SlotSpec* slot_in_row(u8 y) {
    for (int i = 0; i < s_slotRegistryCount; i++) {
        if (s_slotRegistry[i].y == y) return &s_slotRegistry[i];
    }
    return nullptr;
}

// Synthetic id base for a registry entry that uses literal strings. 0xE000+ is
// well clear of the collection menu's real message ids (0x186..0x2a4).
static u16 slot_synth_base(const SlotSpec* s) {
    return static_cast<u16>(0xE000 + (s - s_slotRegistry) * 2);
}

u16 slot_name_id(const SlotSpec* s) {
    return s->name.msgID ? s->name.msgID : slot_synth_base(s);
}

u16 slot_desc_id(const SlotSpec* s) {
    if (s->description.msgID) return s->description.msgID;
    if (s->name.msgID) return static_cast<u16>(s->name.msgID + 0x100);
    return static_cast<u16>(slot_synth_base(s) + 1);
}

const SlotSpec* slot_by_msgid(u32 msgID) {
    for (int i = 0; i < s_slotRegistryCount; i++) {
        const SlotSpec* s = &s_slotRegistry[i];
        if (msgID == slot_name_id(s) || msgID == slot_desc_id(s)) return s;
    }
    return nullptr;
}

ResourceBuffer s_ordonClothesBtiBuf = RESOURCE_BUFFER_INIT;

// Load a packaged .bti (path relative to res/) and return it as a ResTIMG*,
// copied onto the game heap so it outlives the resource buffer. Result cached.
static ResTIMG* load_collection_bti(const char* resPath, ResourceBuffer* buf, ResTIMG** cache) {
    if (*cache != nullptr) return *cache;

    if (buf->data == nullptr) {
        const ResourceService* res_svc = cl_get_resource_service();
        if (res_svc != nullptr && g_modCtx != nullptr) {
            res_svc->load(g_modCtx, resPath, buf);
        }
    }
    if (buf->data == nullptr) return nullptr;

    JKRHeap* gameHeap = mDoExt_getGameHeap();
    if (gameHeap != nullptr && buf->size > 0) {
        void* persistentBuf = gameHeap->alloc(buf->size, 32);
        if (persistentBuf != nullptr) {
            memcpy(persistentBuf, buf->data, buf->size);
            ResTIMG* img = reinterpret_cast<ResTIMG*>(persistentBuf);
            img->alphaEnabled = 1;
            *cache = img;
            return img;
        }
    }
    ResTIMG* img = reinterpret_cast<ResTIMG*>(buf->data);
    img->alphaEnabled = 1;
    *cache = img;
    return img;
}

ResTIMG* get_ordon_clothes_texture() {
    static ResTIMG* s_cache = nullptr;
    return load_collection_bti("textures/ordon_clothes.bti", &s_ordonClothesBtiBuf, &s_cache);
}

ResTIMG* get_ordon_hero_texture() {
    static ResourceBuffer s_buf = RESOURCE_BUFFER_INIT;
    static ResTIMG* s_cache = nullptr;
    return load_collection_bti("textures/clctres/ordonhero.bti", &s_buf, &s_cache);
}

ResTIMG* get_reinforced_shield_texture() {
    static ResourceBuffer s_buf = RESOURCE_BUFFER_INIT;
    static ResTIMG* s_cache = nullptr;
    return load_collection_bti("textures/clctres/reinforced_shield.bti", &s_buf, &s_cache);
}

const ResTIMG* safe_get_tex_info(J2DPane* pane) {
    if (!pane) return nullptr;
    if (pane->getTypeID() == 18 || pane->getTypeID() == 19 || pane->getKind() == 'PIC1' || pane->getKind() == 'PIC2') {
        J2DPicture* pic = reinterpret_cast<J2DPicture*>(pane);
        JUTTexture* tex = pic->getTexture(0);
        if (tex && tex->getTexInfo()) return tex->getTexInfo();
    }
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr; child = child->getNextChildPane()) {
        if (child->getTypeID() == 18 || child->getTypeID() == 19 || child->getKind() == 'PIC1' || child->getKind() == 'PIC2') {
            J2DPicture* pic = reinterpret_cast<J2DPicture*>(child);
            JUTTexture* tex = pic->getTexture(0);
            if (tex && tex->getTexInfo()) return tex->getTexInfo();
        }
    }
    return nullptr;
}

void set_pane_pos(J2DPane* pane, f32 x, f32 y) {
    if (!pane) return;
    pane->translate(x, y);
}

Vec get_pane_center(J2DPane* pane) {
    Vec center = {0.0f, 0.0f, 0.0f};
    if (!pane) return center;

    J2DPane* chain[32];
    int depth = 0;
    for (J2DPane* curr = pane; curr != nullptr && depth < 32; curr = curr->getParentPane()) {
        chain[depth++] = curr;
    }

    Mtx curMtx;
    MTXIdentity(curMtx);
    for (int i = depth - 1; i >= 0; i--) {
        chain[i]->calcMtx();
        Mtx localMtx;
        MTXCopy(*chain[i]->getMtx(), localMtx);
        Mtx next;
        MTXConcat(curMtx, localMtx, next);
        MTXCopy(next, curMtx);
    }

    f32 offsetX = (pane->mBounds.i.x + pane->mBounds.f.x) * 0.5f;
    f32 offsetY = (pane->mBounds.i.y + pane->mBounds.f.y) * 0.5f;

    center.x = curMtx[0][3] + (offsetX * curMtx[0][0] + offsetY * curMtx[0][1]);
    center.y = curMtx[1][3] + (offsetX * curMtx[1][0] + offsetY * curMtx[1][1]);
    center.z = curMtx[2][3];
    return center;
}

void safe_delete_custom_pane(J2DPane*& pane) {
    if (!pane) return;
    if (pane->getParentPane()) {
        pane->getParentPane()->mPaneTree.removeChild(&pane->mPaneTree);
    }
    while (pane->mPaneTree.getFirstChild() != nullptr) {
        JSUTree<J2DPane>* childTree = pane->mPaneTree.getFirstChild();
        J2DPane* child = childTree->getObject();
        pane->mPaneTree.removeChild(childTree);
        delete child;
    }
    delete pane;
    pane = nullptr;
}

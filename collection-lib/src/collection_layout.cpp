#include "collection_internal.hpp"

namespace {

const ClNativeCell kNativeCells[kClRows][3] = {
    {{3, MULTI_CHAR('ken_n0'), MULTI_CHAR('ken_g_0')},
     {4, MULTI_CHAR('ken_n1'), MULTI_CHAR('ken_g_1')},
     {kClNoCell, 0, 0}},
    {{3, MULTI_CHAR('tate_n0'), MULTI_CHAR('tate_g_0')},
     {4, MULTI_CHAR('tate_n1'), MULTI_CHAR('tate_g_1')},
     {kClNoCell, 0, 0}},
    {{3, MULTI_CHAR('fuku_n0'), MULTI_CHAR('fuku_g_0')},
     {4, MULTI_CHAR('fuku_n1'), MULTI_CHAR('fuku_g_1')},
     {5, MULTI_CHAR('fuku_n2'), MULTI_CHAR('fuku_g_2')}},
};
const int kNativeColCount[kClRows] = {2, 2, 3};

const u8 kSpareCells[kClRows][6] = {
    {5, 6, 2, 1, 3, 4},
    {5, 6, 2, 1, 3, 4},
    {6, 2, 1, 3, 4, 5},
};

ClColumn s_cols[kClRows][kClMaxCols + 1];
void (*s_registerFn)() = nullptr;
bool s_registering = false;

constexpr u16 kSynthMsgBase = 0xE000;

int row_index(u8 row) { return (row >= 1 && row <= kClRows) ? row - 1 : -1; }
bool valid_col(u8 col) { return col >= 1 && col <= kClMaxCols; }

bool cell_free(int r, u8 x) {
    if (r == 0 && (x == 5 || x == 6) && !collection_page_claims_cell(x, 0)) return false;
    for (int c = 1; c <= kClMaxCols; c++) {
        if (s_cols[r][c].type != ClColType::Empty && s_cols[r][c].x == x) return false;
    }
    return true;
}

u8 alloc_cell(int r) {
    for (u8 x : kSpareCells[r]) {
        if (cell_free(r, x)) return x;
    }
    return kClNoCell;
}

void reset_native() {
    for (auto& row : s_cols) {
        for (auto& col : row) col = ClColumn{};
    }
    for (int r = 0; r < kClRows; r++) {
        for (int k = 0; k < kNativeColCount[r]; k++) {
            s_cols[r][k + 1] = ClColumn{ClColType::Native, kNativeCells[r][k].x, -1};
        }
    }
}

int first_empty_col(int r) {
    for (int c = 1; c <= kClMaxCols; c++) {
        if (s_cols[r][c].type == ClColType::Empty) return c;
    }
    return 0;
}

int set_custom(int r, int col, const CustomEquipDef& def) {
    if (r < 0 || col < 1 || col > kClMaxCols) {
        log_collect_info("collection-lib: '%s' has no valid slot (row %d, column %d)",
                         def.name ? def.name : "?", r + 1, col);
        return -1;
    }
    ClColumn& column = s_cols[r][col];
    const u8 x = column.type != ClColType::Empty ? column.x : alloc_cell(r);
    if (x == kClNoCell) {
        log_collect_info("collection-lib: no free grid cell for '%s' (row %d, column %d)",
                         def.name ? def.name : "?", r + 1, col);
        return -1;
    }

    CustomEquipDef d = def;
    d.kind = row_kind(r);
    d.item = static_cast<u8>(col);
    const int id = custom_equip_upsert(d);
    if (id < 0) return -1;
    column = ClColumn{ClColType::Custom, x, id};
    return id;
}

int add_next(int r, const CustomEquipDef& def) {
    const int col = first_empty_col(r);
    if (col == 0) {
        log_collect_info("collection-lib: row %d is full, '%s' not added", r + 1,
                         def.name ? def.name : "?");
        return -1;
    }
    return set_custom(r, col, def);
}

bool referenced(int id) {
    for (auto& row : s_cols) {
        for (auto& col : row) {
            if (col.type == ClColType::Custom && col.customId == id) return true;
        }
    }
    return false;
}

}

int native_col_count(int r) { return (r >= 0 && r < kClRows) ? kNativeColCount[r] : 0; }

const ClNativeCell& native_cell(int r, int nativeCol) { return kNativeCells[r][nativeCol - 1]; }

int native_col_of_x(int r, u8 x) {
    for (int k = 0; k < kNativeColCount[r]; k++) {
        if (kNativeCells[r][k].x == x) return k + 1;
    }
    return 0;
}

CustomEquipKind row_kind(int r) { return r == 0 ? CE_SWORD : r == 1 ? CE_SHIELD : CE_TUNIC; }

const ClColumn& layout_column(int r, int col) {
    static const ClColumn kEmpty{};
    if (r < 0 || r >= kClRows || col < 1 || col > kClMaxCols) return kEmpty;
    return s_cols[r][col];
}

int layout_col_of_cell(int r, u8 x) {
    if (r < 0 || r >= kClRows) return 0;
    for (int c = 1; c <= kClMaxCols; c++) {
        if (s_cols[r][c].type != ClColType::Empty && s_cols[r][c].x == x) return c;
    }
    return 0;
}

int layout_last_col(int r) {
    for (int c = kClMaxCols; c >= 1; c--) {
        if (s_cols[r][c].type != ClColType::Empty) return c;
    }
    return 0;
}

bool layout_native_present(int r, u8 nativeX) {
    for (int c = 1; c <= kClMaxCols; c++) {
        if (s_cols[r][c].type == ClColType::Native && s_cols[r][c].x == nativeX) return true;
    }
    return false;
}

int layout_custom_at_cell(int r, u8 x) {
    const int c = layout_col_of_cell(r, x);
    if (c == 0 || s_cols[r][c].type != ClColType::Custom) return -1;
    return s_cols[r][c].customId;
}

void layout_on_custom_removed(int id) {
    for (auto& row : s_cols) {
        for (auto& col : row) {
            if (col.type != ClColType::Custom) continue;
            if (col.customId == id) {
                col = ClColumn{};
            } else if (col.customId > id) {
                col.customId--;
            }
        }
    }
}

bool layout_uses_base_item(u8 itemNo) {
    for (auto& row : s_cols) {
        for (auto& col : row) {
            if (col.type != ClColType::Custom) continue;
            const CustomEquipDef* d = custom_equip_get(col.customId);
            if (d != nullptr && d->baseItem == itemNo) return true;
        }
    }
    return false;
}

bool layout_has_stand_in(u8 itemNo) {
    for (auto& row : s_cols) {
        for (auto& col : row) {
            if (col.type != ClColType::Custom || custom_equip_has_model(col.customId)) continue;
            const CustomEquipDef* d = custom_equip_get(col.customId);
            if (d != nullptr && d->baseItem == itemNo) return true;
        }
    }
    return false;
}

u16 layout_name_msg(int r, u8 x) { return static_cast<u16>(kSynthMsgBase + (r * 7 + x) * 2); }
u16 layout_desc_msg(int r, u8 x) { return static_cast<u16>(layout_name_msg(r, x) + 1); }

bool layout_msg_lookup(u32 msgId, int* r, u8* x, bool* isDesc) {
    if (msgId < kSynthMsgBase || msgId >= kSynthMsgBase + kClRows * 7 * 2) return false;
    const u32 idx = msgId - kSynthMsgBase;
    *isDesc = (idx & 1) != 0;
    *r = static_cast<int>((idx / 2) / 7);
    *x = static_cast<u8>((idx / 2) % 7);
    return true;
}

void collectionlib_run_slot_registration() {
    if (s_registering) return;
    s_registering = true;

    reset_native();
    if (s_registerFn != nullptr) s_registerFn();

    for (int id = custom_equip_count() - 1; id >= 0; --id) {
        if (!referenced(id)) custom_equip_remove(id);
    }

    s_registering = false;
}

void collectionlib_set_register_callback(void (*fn)()) { s_registerFn = fn; }

int CollectionSlotRef::replace(const CustomEquipDef& def) const {
    return set_custom(row_index(row), item, def);
}

int CollectionSlotRef::insert(const CustomEquipDef& def) const {
    const int r = row_index(row);
    if (r < 0 || !valid_col(item)) return -1;
    int gap = item;
    while (gap <= kClMaxCols && s_cols[r][gap].type != ClColType::Empty) gap++;
    if (gap > kClMaxCols || (gap != item && alloc_cell(r) == kClNoCell)) {
        log_collect_info("collection-lib: row %d is full, '%s' not inserted", row,
                         def.name ? def.name : "?");
        return -1;
    }
    for (int c = gap; c > item; c--) s_cols[r][c] = s_cols[r][c - 1];
    s_cols[r][item] = ClColumn{};
    return set_custom(r, item, def);
}

bool CollectionSlotRef::remove() const {
    const int r = row_index(row);
    if (r < 0 || !valid_col(item)) return false;
    s_cols[r][item] = ClColumn{};
    return true;
}

bool CollectionSlotRef::move(u8 newRow, u8 newItem) const {
    const int r = row_index(row);
    if (r < 0 || !valid_col(item) || !valid_col(newItem)) return false;
    if (newRow != row) {
        log_collect_info("collection-lib: slots can only move within their row (%d -> %d)", row, newRow);
        return false;
    }
    const ClColumn tmp = s_cols[r][newItem];
    s_cols[r][newItem] = s_cols[r][item];
    s_cols[r][item] = tmp;
    return true;
}

bool CollectionSlotRef::exists() const {
    return layout_column(row_index(row), item).type != ClColType::Empty;
}

bool CollectionSlotRef::is_native() const {
    return layout_column(row_index(row), item).type == ClColType::Native;
}

Slot get_slot(u8 row, u8 item) { return Slot{row, item}; }
CollectionSlotRef collectionlib_get_slot_ref(u8 row, u8 item) { return CollectionSlotRef{row, item}; }

int collectionlib_add_sword_slot(u8 item, const CustomEquipDef& def) { return set_custom(0, item, def); }
int collectionlib_add_shield_slot(u8 item, const CustomEquipDef& def) { return set_custom(1, item, def); }
int collectionlib_add_tunic_slot(u8 item, const CustomEquipDef& def) { return set_custom(2, item, def); }

int collectionlib_add_slot_override(u8 row, u8 item, const CustomEquipDef& def) {
    return set_custom(row_index(row), item, def);
}

int collectionlib_add_next_sword_slot(const CustomEquipDef& def) { return add_next(0, def); }
int collectionlib_add_next_shield_slot(const CustomEquipDef& def) { return add_next(1, def); }
int collectionlib_add_next_tunic_slot(const CustomEquipDef& def) { return add_next(2, def); }

int collectionlib_register_slot(const CustomEquipDef& def) {
    const int r = static_cast<int>(def.kind);
    if (r < 0 || r >= kClRows) return -1;
    return def.item != 0 ? set_custom(r, def.item, def) : add_next(r, def);
}

int collectionlib_remove_slot(u8 row, u8 item) {
    return CollectionSlotRef{row, item}.remove() ? 0 : -1;
}

int collectionlib_clear_all_slots() {
    for (auto& row : s_cols) {
        for (auto& col : row) col = ClColumn{};
    }
    return 0;
}

CollectionSlot collectionlib_get_slot(u8 row, u8 item) {
    if (row_index(row) < 0 || !valid_col(item)) return CollectionSlot{};
    return CollectionSlot{row, item};
}

bool collectionlib_move_slot(CollectionSlot from, u8 newItem) {
    return CollectionSlotRef{from.row, from.item}.move(from.row, newItem);
}

void collectionlib_reset_layout() { reset_native(); }

void collectionlib_request_reload() { s_needReloadCollect = true; }

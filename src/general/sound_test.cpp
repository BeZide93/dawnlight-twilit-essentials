#include "sound_test.hpp"

#include "mods/service.hpp"
#include "Z2AudioLib/Z2AudioMgr.h"

#include <cstdint>
#include <cstdio>

extern const UiService* svc_ui;
extern ModContext* mod_ctx;

namespace {

struct SeEntry {
    uint64_t id;
    const char* name;
};

#define Z2SEFX(id, name) {static_cast<uint64_t>(id), name},
constexpr SeEntry kSeList[] = {
#include "sound_test_table.inc"
};
#undef Z2SEFX

constexpr size_t kSeCount = sizeof(kSeList) / sizeof(kSeList[0]);

UiListItem s_items[kSeCount];
bool s_itemsBuilt = false;
char s_sectionLabel[48];

constexpr int kPreviewFrames = 120;
u32 s_playingId = 0;
bool s_hasPlaying = false;
int s_stopTimer = 0;

void stop_preview() {
    if (s_hasPlaying) {
        Z2GetAudioMgr()->seStop(static_cast<JAISoundID>(s_playingId), 0);
        s_hasPlaying = false;
    }
    s_stopTimer = 0;
}

void on_sound_row_pressed(ModContext*, UiListHandle, uint64_t item_key, void*) {
    stop_preview();
    s_playingId = static_cast<u32>(item_key);
    Z2GetAudioMgr()->seStart(static_cast<JAISoundID>(s_playingId), nullptr, 0, 0,
                             1.0f, 1.0f, -1.0f, -1.0f, 0);
    s_hasPlaying = true;
    s_stopTimer = kPreviewFrames;
}

}

void sound_test_update() {
    if (s_stopTimer > 0 && --s_stopTimer == 0) {
        stop_preview();
    }
}

ModResult tab_sound_test(ModContext* ctx, UiWindowHandle, UiElementHandle left,
                         UiElementHandle right, void*, ModError*) {
    if (!svc_ui) return MOD_OK;

    if (!s_itemsBuilt) {
        for (size_t i = 0; i < kSeCount; ++i) {
            s_items[i].struct_size = sizeof(UiListItem);
            s_items[i].key = kSeList[i].id;
            s_items[i].label = kSeList[i].name;
        }
        std::snprintf(s_sectionLabel, sizeof(s_sectionLabel),
                      "Sound Effects (%zu)", kSeCount);
        s_itemsBuilt = true;
    }

    svc_ui->pane_add_rml(ctx, right,
        "<p>Every sound effect the game knows (<code>Z2SE_*</code>).</p>"
        "<p>Press a row to play that sound immediately.</p>",
        nullptr);

    svc_ui->pane_add_section(ctx, left, s_sectionLabel);

    UiListDesc desc = UI_LIST_DESC_INIT;
    desc.items = s_items;
    desc.item_count = kSeCount;
    desc.on_pressed = on_sound_row_pressed;

    UiListHandle list = 0;
    svc_ui->pane_add_list(ctx, left, &desc, &list);
    return MOD_OK;
}

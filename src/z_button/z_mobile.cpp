#pragma once

#include "z_mobile.hpp"

#if Z_MOBILE_BUILD

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "mods/svc/config.h"

extern const LogService* svc_log;
extern const ConfigService* svc_config;

#define Z_SYM_MIDNA_SOURCE "_ZN4dusk2ui17midna_icon_sourceEv"
#define Z_SYM_MIDNA_REVISION "_ZN4dusk2ui19midna_icon_revisionEv"
#define Z_SYM_SYNC_DISPLAYS "_ZN4dusk2ui13TouchControls21sync_control_displaysEv"
#define Z_SYM_RML_SET_CLASS                                                                        \
    "_ZN3Rml7Element8SetClassERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb"
#define Z_SYM_RML_SET_INNER_RML                                                                    \
    "_ZN3Rml7Element11SetInnerRMLERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define Z_SYM_RML_GET_CHILD "_ZNK3Rml7Element8GetChildEi"
#define Z_SYM_RML_SET_PROPERTY                                                                     \
    "_ZN3Rml7Element11SetPropertyERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_"
#define Z_SYM_GET_EQUIP_TARGET "_ZN4dusk2ui16get_equip_targetEiRNS0_11EquipTargetE"
#define Z_SYM_TOUCH_DOWN "_ZN4dusk2ui13TouchControls17handle_touch_downERN3Rml5EventE"
#define Z_SYM_TOUCH_UP "_ZN4dusk2ui13TouchControls15handle_touch_upERN3Rml5EventE"
#define Z_SYM_TOUCH_CANCEL "_ZN4dusk2ui13TouchControls19handle_touch_cancelERN3Rml5EventE"
#define Z_SYM_TOUCH_EVENT_ID "_ZN4dusk2ui14touch_event_idERKN3Rml5EventE"
#define Z_SYM_RML_EVENT_TARGET "_ZNK3Rml5Event16GetTargetElementEv"
#define Z_SYM_RML_GET_PARENT "_ZNK3Rml7Element13GetParentNodeEv"
#define Z_SYM_RML_OWNER_DOCUMENT "_ZNK3Rml7Element16GetOwnerDocumentEv"
#define Z_SYM_RML_CREATE_ELEMENT                                                                   \
    "_ZN3Rml15ElementDocument13CreateElementERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define Z_SYM_RML_APPEND_CHILD                                                                     \
    "_ZN3Rml7Element11AppendChildENSt6__ndk110unique_ptrIS0_NS_8ReleaserIS0_EEEEb"
#define Z_SYM_RML_GET_ELEMENT_BY_ID                                                                \
    "_ZN3Rml7Element14GetElementByIdERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define Z_SYM_RML_SET_ID                                                                           \
    "_ZN3Rml7Element5SetIdERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define Z_SYM_RML_SET_PSEUDO_CLASS                                                                 \
    "_ZN3Rml7Element14SetPseudoClassERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb"
#define Z_SYM_RML_IS_PSEUDO_CLASS_SET                                                              \
    "_ZNK3Rml7Element16IsPseudoClassSetERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define Z_SYM_RML_RELEASE_TEXTURE                                                                  \
    "_ZN3Rml14ReleaseTextureERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEEPNS_15RenderInterfaceE"
#define Z_SYM_RML_ADD_EVENT_LISTENER                                                               \
    "_ZN3Rml7Element16AddEventListenerERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEPNS_13EventListenerEb"
#define Z_SYM_RML_STOP_PROPAGATION "_ZN3Rml5Event15StopPropagationEv"
#define Z_SYM_TOUCH_EVENT_POSITION "_ZN4dusk2ui20touch_event_positionERKN3Rml5EventE"
#define Z_SYM_TOUCH_DP_SCALE "_ZN4dusk2ui14touch_dp_scaleEPN3Rml7ContextE"
#define Z_SYM_TOUCH_DOCUMENT_SIZE "_ZN4dusk2ui22touch_document_size_dpEPN3Rml7ContextE"
#define Z_SYM_GET_CONTEXT "_ZN6aurora5rmlui11get_contextEv"
#define Z_SYM_DOCK_CLASSES "_ZN4dusk2ui26apply_control_dock_classesEPN3Rml7ElementENS0_13ControlAnchorE"
#define Z_SYM_EDITOR_SYNC_LAYOUTS "_ZN4dusk2ui19TouchControlsEditor20sync_control_layoutsEv"
#define Z_SYM_EDITOR_SAVE "_ZN4dusk2ui19TouchControlsEditor11save_layoutEv"
#define Z_SYM_EDITOR_RESET "_ZN4dusk2ui19TouchControlsEditor20reset_working_layoutEv"
#define Z_SYM_EDITOR_DTOR "_ZN4dusk2ui19TouchControlsEditorD2Ev"

DEFINE_HOOK_SYMBOL(Z_SYM_MIDNA_SOURCE, std::string(), ZMidnaIconSourceHook);
DEFINE_HOOK_SYMBOL(Z_SYM_MIDNA_REVISION, uint64_t(), ZMidnaIconRevisionHook);
DEFINE_HOOK_SYMBOL(Z_SYM_SYNC_DISPLAYS, void(void*), ZTouchSyncDisplaysHook);
DEFINE_HOOK_SYMBOL(Z_SYM_RML_SET_CLASS, void(void*, const std::string*, bool), ZRmlSetClassHook);
DEFINE_HOOK_SYMBOL(Z_SYM_TOUCH_DOWN, void(void*, void*), ZTouchDownHook);
DEFINE_HOOK_SYMBOL(Z_SYM_TOUCH_UP, void(void*, void*), ZTouchUpHook);
DEFINE_HOOK_SYMBOL(Z_SYM_TOUCH_CANCEL, void(void*, void*), ZTouchCancelHook);
DEFINE_HOOK(&mDoCPd_c::read, ZMobilePadReadHook);
DEFINE_HOOK(&dMeter2Draw_c::setButtonIconMidonaAlpha, ZMobileMidonaAlphaHook);
DEFINE_HOOK_SYMBOL(Z_SYM_DOCK_CLASSES, void(void*, u8), ZDockClassesHook);
DEFINE_HOOK_SYMBOL(Z_SYM_EDITOR_SYNC_LAYOUTS, void(void*), ZEditorSyncLayoutsHook);
DEFINE_HOOK_SYMBOL(Z_SYM_EDITOR_SAVE, void(void*), ZEditorSaveHook);
DEFINE_HOOK_SYMBOL(Z_SYM_EDITOR_RESET, void(void*), ZEditorResetHook);
DEFINE_HOOK_SYMBOL(Z_SYM_EDITOR_DTOR, void(void*), ZEditorDtorHook);

namespace {

using RmlGetChildFn = void* (*)(const void*, int);
using RmlSetPropertyFn = bool (*)(void*, const std::string*, const std::string*);
using RmlSetInnerRMLFn = void (*)(void*, const std::string*);
using RmlSetClassFn = void (*)(void*, const std::string*, bool);
using RmlGetNodeFn = void* (*)(const void*);
using RmlGetElementByIdFn = void* (*)(void*, const std::string*);
using RmlSetIdFn = void (*)(void*, const std::string*);
using RmlSetPseudoClassFn = void (*)(void*, const std::string*, bool);
using RmlIsPseudoClassSetFn = bool (*)(const void*, const std::string*);
using TouchEventIdFn = uint64_t (*)(const void*);
using RmlReleaseTextureFn = void (*)(const std::string*, void*);
using RmlAddEventListenerFn = void (*)(void*, const std::string*, void*, bool);
using RmlStopPropagationFn = void (*)(void*);
struct Vec2fABI {
    float x;
    float y;
};
using TouchEventPositionFn = Vec2fABI (*)(const void*);
using TouchDocumentSizeFn = Vec2fABI (*)(void*);
using TouchDpScaleFn = float (*)(void*);
using GetContextFn = void* (*)();

struct RmlElementPtr {
    void* element = nullptr;
    ~RmlElementPtr() {}
};
using RmlCreateElementFn = RmlElementPtr (*)(void*, const std::string*);
using RmlAppendChildFn = void* (*)(void*, RmlElementPtr*, bool);

struct EquipTargetABI {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool valid = false;
};
using GetEquipTargetFn = bool (*)(int, EquipTargetABI&);

const HookService* s_hookSvc = nullptr;
RmlGetChildFn s_rmlGetChild = nullptr;
RmlSetPropertyFn s_rmlSetProperty = nullptr;
RmlSetInnerRMLFn s_rmlSetInnerRML = nullptr;
GetEquipTargetFn s_getEquipTarget = nullptr;
RmlSetClassFn s_rmlSetClass = nullptr;
RmlGetNodeFn s_rmlGetParentNode = nullptr;
RmlGetNodeFn s_rmlGetOwnerDocument = nullptr;
RmlGetNodeFn s_rmlEventTarget = nullptr;
RmlCreateElementFn s_rmlCreateElement = nullptr;
RmlAppendChildFn s_rmlAppendChild = nullptr;
RmlGetElementByIdFn s_rmlGetElementById = nullptr;
RmlSetIdFn s_rmlSetId = nullptr;
RmlSetPseudoClassFn s_rmlSetPseudoClass = nullptr;
RmlIsPseudoClassSetFn s_rmlIsPseudoClassSet = nullptr;
TouchEventIdFn s_touchEventId = nullptr;
RmlReleaseTextureFn s_rmlReleaseTexture = nullptr;
RmlAddEventListenerFn s_rmlAddEventListener = nullptr;
RmlStopPropagationFn s_rmlStopPropagation = nullptr;
TouchEventPositionFn s_touchEventPosition = nullptr;
TouchDocumentSizeFn s_touchDocumentSize = nullptr;
TouchDpScaleFn s_touchDpScale = nullptr;
GetContextFn s_getContext = nullptr;

bool s_inDisplaySync = false;
void* s_syncZButton = nullptr;

void* s_meterButton = nullptr;
void* s_meterContainer = nullptr;
std::string s_meterRml;

bool s_useCaptureFallback = false;
J2DPane* s_hostedMidona = nullptr;
bool s_hostActive = false;

constexpr const char* kMidnaButtonId = "twe-midna-touch";
bool s_midnaButtonApiOk = false;
void* s_midnaButton = nullptr;
bool s_midnaButtonShown = false;
bool s_midnaButtonFilled = false;
std::string s_midnaButtonSource;
bool s_midnaTouchActive = false;
uint64_t s_midnaTouchFinger = 0;
bool s_midnaTouchPressed = false;
bool s_midnaTouchTrig = false;
bool s_touchZShown = false;

struct MidnaButtonPos {
    double x = -1.0;
    double y = -1.0;
};
constexpr float kMidnaButtonW = 78.0f;
constexpr float kMidnaButtonH = 46.0f;
constexpr float kMidnaDefaultLeft = 24.0f;
constexpr float kMidnaDefaultTop = 72.0f;
ConfigVarHandle s_varMidnaPosX = 0;
ConfigVarHandle s_varMidnaPosY = 0;
MidnaButtonPos s_midnaSavedPos;
Vec2fABI s_midnaPlaced{-1.0f, -1.0f};

struct EditorDrag {
    bool active = false;
    uint64_t finger = 0;
    Vec2fABI startPx{};
    Vec2fABI startDp{};
};
bool s_midnaEditorApiOk = false;
bool s_inEditorSync = false;
void* s_editorElement = nullptr;
void* s_editor = nullptr;
void* s_editorButton = nullptr;
MidnaButtonPos s_midnaEditorPos;
Vec2fABI s_editorPlaced{-1.0f, -1.0f};
EditorDrag s_editorDrag;

struct PaneRenderState {
    J2DPane* pane;
    u8 alpha;
    bool visible;
};
std::array<PaneRenderState, 32> s_forcedMidnaPanes{};
size_t s_forcedMidnaPaneCount = 0;

template <typename Fn>
bool resolve_symbol(const char* name, Fn& out) {
    if (s_hookSvc == nullptr || s_hookSvc->resolve == nullptr) {
        return false;
    }
    void* addr = nullptr;
    if (s_hookSvc->resolve(mod_ctx, name, &addr, nullptr) != MOD_OK || addr == nullptr) {
        return false;
    }
    out = reinterpret_cast<Fn>(addr);
    return true;
}

bool twilight_hd_touch_z() {
    return s_midnaButtonApiOk && !g_configCustomZButtonEnabled && twilight_hd_third_item_slot();
}

bool midna_callable() {
    return dComIfGs_isEventBit(dSv_event_flag_c::M_067) &&
           !dComIfGs_isEventBit(dSv_event_flag_c::F_0800);
}

bool touch_z_item_mode() {
    return g_configCustomZButtonEnabled || twilight_hd_touch_z();
}

u8 current_z_item() {
    if (isWolfPlayer()) {
        return dItemNo_NONE_e;
    }
    if (twilight_hd_touch_z()) {
        const u8 slot = dComIfGs_getSelectItemIndex(2);
        if (slot >= 24) {
            return dItemNo_NONE_e;
        }
        const u8 item =
            combine_select_item(dComIfGs_getItem(slot, false), dComIfGs_getMixItemIndex(2));
        return (item == 0xFF || item == 0x00) ? dItemNo_NONE_e : item;
    }
    u8 zItem = resolved_select_item(2);
    if (zItem == 0xFF || zItem == 0x00 || zItem == dItemNo_NONE_e) {
        if (g_zInventorySlot != 0xFF && g_zInventorySlot < 24) {
            zItem = dComIfGs_getItem(g_zInventorySlot, false);
        }
    }
    if (zItem == 0xFF || zItem == 0x00) {
        return dItemNo_NONE_e;
    }
    return zItem;
}

std::string z_item_icon_source() {
    const u8 itemNo = current_z_item();
    if (itemNo == dItemNo_NONE_e) {
        return {};
    }
    const u8 textureItem = (itemNo == dItemNo_LIGHT_ARROW_e) ? dItemNo_BOW_e : itemNo;
    char buf[48] = {};
    std::snprintf(buf, sizeof(buf), "item://item/%02x?ztwe=%02x", textureItem, itemNo);
    return buf;
}

void after_midna_icon_source(ModContext*, void*, void* retval, void*) {
    if (!touch_z_item_mode() || retval == nullptr) {
        return;
    }
    *static_cast<std::string*>(retval) = z_item_icon_source();
}

void after_midna_icon_revision(ModContext*, void*, void* retval, void*) {
    if (!touch_z_item_mode() || retval == nullptr) {
        return;
    }
    const u8 itemNo = current_z_item();
    if (itemNo == dItemNo_NONE_e) {
        *static_cast<uint64_t*>(retval) = 0;
        return;
    }
    int count = 0;
    int maxCount = 0;
    z_item_ammo(itemNo, count, maxCount);
    *static_cast<uint64_t*>(retval) = 0x5A000000ull | (static_cast<uint64_t>(itemNo) << 12) |
                                      static_cast<uint64_t>(count & 0xFFF);
}

void set_property(void* element, const char* name, const char* value) {
    if (element == nullptr || s_rmlSetProperty == nullptr) {
        return;
    }
    const std::string propertyName = name;
    const std::string propertyValue = value;
    s_rmlSetProperty(element, &propertyName, &propertyValue);
}

void configure_meter_container(void* container) {
    set_property(container, "position", "absolute");
    set_property(container, "left", "0dp");
    set_property(container, "top", "0dp");
    set_property(container, "right", "auto");
    set_property(container, "bottom", "auto");
    set_property(container, "width", "100%");
    set_property(container, "height", "100%");
    set_property(container, "font-size", "0dp");
    set_property(container, "overflow", "visible");
    set_property(container, "pointer-events", "none");
}

void sync_z_button_meter(void* button) {
    if (button == nullptr || s_rmlGetChild == nullptr || s_rmlSetProperty == nullptr ||
        s_rmlSetInnerRML == nullptr)
    {
        return;
    }

    void* container = s_rmlGetChild(button, 1);
    if (container == nullptr) {
        return;
    }
    if (button != s_meterButton || container != s_meterContainer) {
        s_meterButton = button;
        s_meterContainer = container;
        s_meterRml.clear();
        configure_meter_container(container);
    }

    const u8 itemNo = touch_z_item_mode() ? current_z_item() : dItemNo_NONE_e;
    std::string rml =
        touch_z_item_mode() && itemNo == dItemNo_NONE_e
            ? "<span style=\"position:absolute;left:0dp;top:50%;width:100%;margin-top:-11dp;"
              "text-align:center;font-size:22dp;line-height:1;\">Z</span>"
            : "<span style=\"position:absolute;right:9dp;bottom:7dp;font-size:13dp;"
              "line-height:1;\">Z</span>";

    if (itemNo != dItemNo_NONE_e) {
        int count = 0;
        int maxCount = 0;
        if (z_item_ammo(itemNo, count, maxCount) && count >= 0) {
            rml += "<count class=\"item-count visible\">" + std::to_string(count) + "</count>";
        } else if (z_item_is_lantern(itemNo) && dComIfGs_getMaxOil() > 0) {
            const f32 fill = std::clamp(static_cast<f32>(dComIfGs_getOil()) /
                                            static_cast<f32>(dComIfGs_getMaxOil()),
                0.0f, 1.0f);
            char percent[32] = {};
            std::snprintf(percent, sizeof(percent), "%.1f%%", fill * 100.0f);
            rml += "<oil-meter class=\"oil-meter visible\"><oil-fill style=\"width:" +
                   std::string(percent) + ";\" /></oil-meter>";
        }
    }

    if (rml == s_meterRml) {
        return;
    }
    s_rmlSetInnerRML(container, &rml);
    s_meterRml = rml;
}

std::string true_midna_icon_source() {
    return ZMidnaIconSourceHook::g_orig != nullptr ? ZMidnaIconSourceHook::g_orig() : std::string();
}

void set_midna_button_pressed(bool pressed) {
    if (s_midnaButton == nullptr) {
        return;
    }
    const std::string pressedClass = "pressed";
    s_rmlSetClass(s_midnaButton, &pressedClass, pressed);
}

void set_midna_button_shown(bool shown) {
    if (s_midnaButton == nullptr || shown == s_midnaButtonShown) {
        return;
    }
    const std::string hiddenClass = "hidden";
    s_rmlSetPseudoClass(s_midnaButton, &hiddenClass, !shown);
    s_midnaButtonShown = shown;
    if (!shown && s_midnaTouchActive) {
        s_midnaTouchActive = false;
        set_midna_button_pressed(false);
    }
}

std::string midna_button_rml(const std::string& source) {
    return source.empty() ? "<span style=\"font-size:16dp;\">Midna</span>"
                          : "<img class=\"midna-icon visible\" src=\"" + source + "\" />";
}

bool touch_document_size(float& w, float& h) {
    if (s_touchDocumentSize == nullptr || s_getContext == nullptr) {
        return false;
    }
    void* context = s_getContext();
    if (context == nullptr) {
        return false;
    }
    const Vec2fABI size = s_touchDocumentSize(context);
    w = size.x;
    h = size.y;
    return w > kMidnaButtonW && h > kMidnaButtonH;
}

Vec2fABI midna_button_corner(const MidnaButtonPos& pos) {
    float w = 0.0f;
    float h = 0.0f;
    if (pos.x < 0.0 || pos.y < 0.0 || !touch_document_size(w, h)) {
        return {kMidnaDefaultLeft, kMidnaDefaultTop};
    }
    return {
        std::clamp(static_cast<float>(pos.x) * w - kMidnaButtonW * 0.5f, 0.0f, w - kMidnaButtonW),
        std::clamp(static_cast<float>(pos.y) * h - kMidnaButtonH * 0.5f, 0.0f, h - kMidnaButtonH),
    };
}

void place_midna_button(void* button, const MidnaButtonPos& pos, Vec2fABI& placed) {
    const Vec2fABI corner = midna_button_corner(pos);
    if (corner.x == placed.x && corner.y == placed.y) {
        return;
    }
    char value[32] = {};
    std::snprintf(value, sizeof(value), "%.2fdp", corner.x);
    set_property(button, "left", value);
    std::snprintf(value, sizeof(value), "%.2fdp", corner.y);
    set_property(button, "top", value);
    placed = corner;
}

void* create_midna_button(void* sibling, void* parent) {
    void* document = s_rmlGetOwnerDocument(sibling);
    if (document == nullptr) {
        return nullptr;
    }
    const std::string tag = "button";
    RmlElementPtr created = s_rmlCreateElement(document, &tag);
    if (created.element == nullptr) {
        return nullptr;
    }
    void* button = s_rmlAppendChild(parent, &created, true);
    if (button == nullptr) {
        return nullptr;
    }

    const std::string id = kMidnaButtonId;
    s_rmlSetId(button, &id);
    for (const char* name : {"control", "trigger", "button-z"}) {
        const std::string className = name;
        s_rmlSetClass(button, &className, true);
    }
    set_property(button, "width", "78dp");
    set_property(button, "height", "46dp");
    return button;
}

void sync_midna_button(void* zButton) {
    if (!s_midnaButtonApiOk) {
        return;
    }

    const std::string source = true_midna_icon_source();
    const std::string hiddenClass = "hidden";
    s_touchZShown = zButton != nullptr && !s_rmlIsPseudoClassSet(zButton, &hiddenClass);
    const bool wanted = s_touchZShown && touch_z_item_mode() &&
                        dMeter2Info_getWindowStatus() == 0 && !isTitleOrMainMenu() &&
                        midna_callable();
    if (!wanted) {
        set_midna_button_shown(false);
        return;
    }

    void* parent = s_rmlGetParentNode(zButton);
    if (parent == nullptr) {
        set_midna_button_shown(false);
        return;
    }
    const std::string id = kMidnaButtonId;
    void* button = s_rmlGetElementById(parent, &id);
    if (button == nullptr) {
        button = create_midna_button(zButton, parent);
        if (button == nullptr) {
            return;
        }
    }
    if (button != s_midnaButton) {
        s_midnaButton = button;
        s_midnaButtonShown = false;
        s_midnaButtonFilled = false;
        s_midnaButtonSource.clear();
        s_midnaTouchActive = false;
        s_midnaPlaced = {-1.0f, -1.0f};
    }
    place_midna_button(button, s_midnaSavedPos, s_midnaPlaced);

    if (!s_midnaButtonFilled || source != s_midnaButtonSource) {
        const std::string rml = midna_button_rml(source);
        s_rmlSetInnerRML(button, &rml);
        if (s_rmlReleaseTexture != nullptr && !s_midnaButtonSource.empty()) {
            s_rmlReleaseTexture(&s_midnaButtonSource, nullptr);
        }
        s_midnaButtonSource = source;
        s_midnaButtonFilled = true;
    }
    set_midna_button_shown(true);
}

bool event_hits_midna_button(void* event) {
    if (!s_midnaButtonShown || s_midnaButton == nullptr || event == nullptr) {
        return false;
    }
    void* element = s_rmlEventTarget(event);
    for (int depth = 0; element != nullptr && depth < 4; ++depth) {
        if (element == s_midnaButton) {
            return true;
        }
        element = s_rmlGetParentNode(element);
    }
    return false;
}

HookAction before_touch_down(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    if (!event_hits_midna_button(event)) {
        return HOOK_CONTINUE;
    }
    if (!s_midnaTouchActive) {
        s_midnaTouchActive = true;
        s_midnaTouchFinger = s_touchEventId(event);
        s_midnaTouchPressed = true;
        set_midna_button_pressed(true);
    }
    return HOOK_SKIP_ORIGINAL;
}

HookAction before_touch_release(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    if (!s_midnaTouchActive || event == nullptr || s_touchEventId(event) != s_midnaTouchFinger) {
        return HOOK_CONTINUE;
    }
    s_midnaTouchActive = false;
    set_midna_button_pressed(false);
    return HOOK_SKIP_ORIGINAL;
}

void after_pad_read_midna_touch(ModContext*, void*, void*, void*) {
    s_midnaTouchTrig = s_midnaTouchPressed;
    s_midnaTouchPressed = false;
}

void force_pane_tree_visible(J2DPane* pane) {
    if (pane == nullptr || s_forcedMidnaPaneCount >= s_forcedMidnaPanes.size()) {
        return;
    }
    s_forcedMidnaPanes[s_forcedMidnaPaneCount++] = {pane, pane->getAlpha(), pane->isVisible()};
    pane->show();
    pane->setAlpha(255);
    for (J2DPane* child = pane->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        force_pane_tree_visible(child);
    }
}

HookAction before_midona_alpha(ModContext*, void* args, void*, void*) {
    s_forcedMidnaPaneCount = 0;
    if (!s_touchZShown || !twilight_hd_touch_z() || !midna_callable() || args == nullptr) {
        return HOOK_CONTINUE;
    }
    dMeter2Draw_c* draw = mods::arg<dMeter2Draw_c*>(args, 0);
    J2DPane* midona = (draw != nullptr && draw->mpButtonMidona != nullptr)
                          ? draw->mpButtonMidona->getPanePtr()
                          : nullptr;
    if (midona != nullptr && !midona->isVisible()) {
        force_pane_tree_visible(midona);
    }
    return HOOK_CONTINUE;
}

void after_midona_alpha(ModContext*, void*, void*, void*) {
    while (s_forcedMidnaPaneCount > 0) {
        const PaneRenderState& state = s_forcedMidnaPanes[--s_forcedMidnaPaneCount];
        state.pane->setAlpha(state.alpha);
        if (state.visible) {
            state.pane->show();
        } else {
            state.pane->hide();
        }
    }
}

void on_editor_button_touch(int kind, void* event);

class MidnaEditorListener {
public:
    enum Kind { Start, Move, End, Cancel };
    explicit MidnaEditorListener(Kind kind) : mKind(kind) {}
    virtual ~MidnaEditorListener() {}
    virtual void ProcessEvent(void* event) { on_editor_button_touch(mKind, event); }
    virtual void OnAttach(void*) {}
    virtual void OnDetach(void*) {}

private:
    void* mObserverBlock = nullptr;
    Kind mKind;
};

MidnaEditorListener s_editorTouchStart{MidnaEditorListener::Start};
MidnaEditorListener s_editorTouchMove{MidnaEditorListener::Move};
MidnaEditorListener s_editorTouchEnd{MidnaEditorListener::End};
MidnaEditorListener s_editorTouchCancel{MidnaEditorListener::Cancel};

void set_editor_button_selected(bool selected) {
    const std::string selectedClass = "editor-selected";
    s_rmlSetClass(s_editorButton, &selectedClass, selected);
}

void on_editor_button_touch(int kind, void* event) {
    if (s_editorButton == nullptr || event == nullptr) {
        return;
    }
    const uint64_t finger = s_touchEventId(event);
    const Vec2fABI position = s_touchEventPosition(event);
    if (kind == MidnaEditorListener::Start) {
        if (!s_editorDrag.active) {
            s_editorDrag = {.active = true, .finger = finger, .startPx = position,
                            .startDp = s_editorPlaced};
            set_editor_button_selected(true);
        }
    } else if (s_editorDrag.active && finger == s_editorDrag.finger) {
        float w = 0.0f;
        float h = 0.0f;
        if (kind == MidnaEditorListener::Move && touch_document_size(w, h)) {
            const float scale = std::max(s_touchDpScale(s_getContext()), 1.0f);
            const float left = std::clamp(
                s_editorDrag.startDp.x + (position.x - s_editorDrag.startPx.x) / scale, 0.0f,
                w - kMidnaButtonW);
            const float top = std::clamp(
                s_editorDrag.startDp.y + (position.y - s_editorDrag.startPx.y) / scale, 0.0f,
                h - kMidnaButtonH);
            s_midnaEditorPos = {(left + kMidnaButtonW * 0.5f) / w, (top + kMidnaButtonH * 0.5f) / h};
            place_midna_button(s_editorButton, s_midnaEditorPos, s_editorPlaced);
        } else if (kind == MidnaEditorListener::End || kind == MidnaEditorListener::Cancel) {
            s_editorDrag = {};
            set_editor_button_selected(false);
        }
    }
    s_rmlStopPropagation(event);
}

void attach_editor_listener(void* button, const char* type, MidnaEditorListener& listener) {
    const std::string eventType = type;
    s_rmlAddEventListener(button, &eventType, &listener, false);
}

void sync_editor_midna_button(void* editor, void* element) {
    void* parent = s_rmlGetParentNode(element);
    if (parent == nullptr) {
        return;
    }
    const std::string id = kMidnaButtonId;
    void* existing = s_rmlGetElementById(parent, &id);
    if (editor != s_editor || existing != s_editorButton) {
        s_editor = editor;
        s_editorButton = nullptr;
        s_editorPlaced = {-1.0f, -1.0f};
        s_editorDrag = {};
        s_midnaEditorPos = s_midnaSavedPos;
    }

    const std::string hiddenClass = "hidden";
    if (!touch_z_item_mode()) {
        if (existing != nullptr) {
            s_rmlSetPseudoClass(existing, &hiddenClass, true);
        }
        return;
    }
    if (s_editorButton == nullptr) {
        void* button = existing != nullptr ? existing : create_midna_button(element, parent);
        if (button == nullptr) {
            return;
        }
        const std::string rml = midna_button_rml(true_midna_icon_source());
        s_rmlSetInnerRML(button, &rml);
        attach_editor_listener(button, "touchstart", s_editorTouchStart);
        attach_editor_listener(button, "touchmove", s_editorTouchMove);
        attach_editor_listener(button, "touchend", s_editorTouchEnd);
        attach_editor_listener(button, "touchcancel", s_editorTouchCancel);
        s_editorButton = button;
    }
    s_rmlSetPseudoClass(s_editorButton, &hiddenClass, false);
    place_midna_button(s_editorButton, s_midnaEditorPos, s_editorPlaced);
}

HookAction before_editor_sync_layouts(ModContext*, void*, void*, void*) {
    s_inEditorSync = true;
    s_editorElement = nullptr;
    return HOOK_CONTINUE;
}

HookAction before_dock_classes(ModContext*, void* args, void*, void*) {
    if (s_inEditorSync && s_editorElement == nullptr && args != nullptr) {
        s_editorElement = mods::arg<void*>(args, 0);
    }
    return HOOK_CONTINUE;
}

void after_editor_sync_layouts(ModContext*, void* args, void*, void*) {
    s_inEditorSync = false;
    void* element = s_editorElement;
    s_editorElement = nullptr;
    void* editor = args != nullptr ? mods::arg<void*>(args, 0) : nullptr;
    if (editor != nullptr && element != nullptr) {
        sync_editor_midna_button(editor, element);
    }
}

HookAction before_editor_save(ModContext*, void* args, void*, void*) {
    if (args == nullptr || s_editor == nullptr || mods::arg<void*>(args, 0) != s_editor) {
        return HOOK_CONTINUE;
    }
    s_midnaSavedPos = s_midnaEditorPos;
    if (svc_config != nullptr && s_varMidnaPosX != 0 && s_varMidnaPosY != 0) {
        svc_config->set_float(mod_ctx, s_varMidnaPosX, s_midnaSavedPos.x);
        svc_config->set_float(mod_ctx, s_varMidnaPosY, s_midnaSavedPos.y);
    }
    return HOOK_CONTINUE;
}

void after_editor_reset(ModContext*, void* args, void*, void*) {
    if (args == nullptr || s_editor == nullptr || mods::arg<void*>(args, 0) != s_editor) {
        return;
    }
    s_midnaEditorPos = {};
    s_editorDrag = {};
    if (s_editorButton != nullptr) {
        place_midna_button(s_editorButton, s_midnaEditorPos, s_editorPlaced);
    }
}

HookAction before_editor_destroy(ModContext*, void* args, void*, void*) {
    if (args != nullptr && mods::arg<void*>(args, 0) == s_editor) {
        s_editor = nullptr;
        s_editorButton = nullptr;
        s_editorDrag = {};
    }
    return HOOK_CONTINUE;
}

void register_midna_position_vars() {
    if (svc_config == nullptr) {
        return;
    }
    ConfigVarDesc desc = CONFIG_VAR_DESC_INIT;
    desc.type = CONFIG_VAR_FLOAT;
    desc.default_float = -1.0;
    desc.name = "midnaTouchButtonX";
    if (svc_config->register_var(mod_ctx, &desc, &s_varMidnaPosX) == MOD_OK) {
        svc_config->get_float(mod_ctx, s_varMidnaPosX, &s_midnaSavedPos.x);
    }
    desc.name = "midnaTouchButtonY";
    if (svc_config->register_var(mod_ctx, &desc, &s_varMidnaPosY) == MOD_OK) {
        svc_config->get_float(mod_ctx, s_varMidnaPosY, &s_midnaSavedPos.y);
    }
}

HookAction before_touch_sync_displays(ModContext*, void*, void*, void*) {
    s_inDisplaySync = true;
    s_syncZButton = nullptr;
    return HOOK_CONTINUE;
}

void after_touch_sync_displays(ModContext*, void*, void*, void*) {
    void* button = s_syncZButton;
    s_inDisplaySync = false;
    s_syncZButton = nullptr;
    sync_z_button_meter(button);
    sync_midna_button(button);
}

HookAction before_rml_set_class(ModContext*, void* args, void*, void*) {
    if (!s_inDisplaySync || args == nullptr) {
        return HOOK_CONTINUE;
    }
    void* element = mods::arg<void*>(args, 0);
    const std::string* className = mods::arg<const std::string*>(args, 1);
    if (element != nullptr && className != nullptr && *className == "has-icon") {
        s_syncZButton = element;
    }
    return HOOK_CONTINUE;
}

void release_hosted_midona() {
    if (!s_hostActive) {
        return;
    }
    if (s_hostedMidona != nullptr) {
        for (J2DPane* child = s_hostedMidona->getFirstChildPane(); child != nullptr;
             child = child->getNextChildPane()) {
            child->show();
        }
    }
    s_hostedMidona = nullptr;
    s_hostActive = false;
}

}

bool z_mobile_active() {
    return true;
}

bool z_mobile_consume_midna_touch() {
    const bool triggered = s_midnaTouchTrig;
    s_midnaTouchTrig = false;
    return triggered;
}

bool z_mobile_twilight_hd_touch_z() {
    return s_touchZShown && twilight_hd_touch_z();
}

bool z_mobile_twilight_hd_z_is_item() {
    return z_mobile_twilight_hd_touch_z() && current_z_item() != dItemNo_NONE_e;
}

bool z_mobile_wants_midona_host() {
    return s_useCaptureFallback && g_configCustomZButtonEnabled && !isWolfPlayer() &&
           !is_pause_menu_open() && current_z_item() != dItemNo_NONE_e;
}

J2DPane* z_mobile_sync_touch_z(dMeter2Draw_c* draw) {
    if (!s_useCaptureFallback || draw == nullptr || isTitleOrMainMenu()) {
        return nullptr;
    }
    J2DScreen* screen = draw->getMainScreenPtr();
    if (screen == nullptr) {
        release_hosted_midona();
        return nullptr;
    }

    J2DPane* midona = screen->search(MULTI_CHAR('midona_n'));
    const bool wantHost = g_configCustomZButtonEnabled && !isWolfPlayer() &&
                          !is_pause_menu_open(draw) && current_z_item() != dItemNo_NONE_e;
    if (!wantHost || midona == nullptr) {
        release_hosted_midona();
        return nullptr;
    }

    J2DPane* zbtn = screen->search(MULTI_CHAR('zbtn_n'));
    if (zbtn != nullptr && midona->getParentPane() != zbtn) {
        J2DPane* oldParent = midona->getParentPane();
        if (oldParent != nullptr) {
            oldParent->mPaneTree.removeChild(&midona->mPaneTree);
        }
        zbtn->appendChild(midona);
        midona->translate(0.0f, 0.0f);
    }

    g_meter2_info.onUseButton(0x800);
    draw->mButtonZAlpha = 1.0f;
    if (draw->mpButtonParent != nullptr) {
        draw->mpButtonParent->setAlphaRate(1.0f);
    }

    midona->show();
    midona->setAlpha(255);

    J2DPane* itemPane = nullptr;
    if (CPaneMgr* itemR = dMeter2Info_getMeterItemPanePtr(2)) {
        itemPane = itemR->getPanePtr();
    }
    for (J2DPane* child = midona->getFirstChildPane(); child != nullptr;
         child = child->getNextChildPane()) {
        if (child == itemPane) {
            child->show();
            continue;
        }
        child->hide();
        child->setAlpha(0);
    }

    s_hostedMidona = midona;
    s_hostActive = true;
    return midona;
}

void z_mobile_init(const HookService* hook_svc) {
    s_hookSvc = hook_svc;
    if (hook_svc == nullptr) {
        return;
    }

    const bool iconOk =
        mods::hook::add_post<ZMidnaIconSourceHook>(hook_svc, after_midna_icon_source) == MOD_OK &&
        mods::hook::add_post<ZMidnaIconRevisionHook>(hook_svc, after_midna_icon_revision) == MOD_OK;

    s_useCaptureFallback = !iconOk;

    const bool meterOk =
        resolve_symbol(Z_SYM_RML_GET_CHILD, s_rmlGetChild) &&
        resolve_symbol(Z_SYM_RML_SET_PROPERTY, s_rmlSetProperty) &&
        resolve_symbol(Z_SYM_RML_SET_INNER_RML, s_rmlSetInnerRML) &&
        mods::hook::add_pre<ZTouchSyncDisplaysHook>(hook_svc, before_touch_sync_displays) ==
            MOD_OK &&
        mods::hook::add_post<ZTouchSyncDisplaysHook>(hook_svc, after_touch_sync_displays) ==
            MOD_OK &&
        mods::hook::add_pre<ZRmlSetClassHook>(hook_svc, before_rml_set_class) == MOD_OK;

    resolve_symbol(Z_SYM_GET_EQUIP_TARGET, s_getEquipTarget);

    resolve_symbol(Z_SYM_RML_RELEASE_TEXTURE, s_rmlReleaseTexture);
    const HookOptions afterTwilightHd = twilight_hd_hook_order(kTwilightHdRunAfter);
    s_midnaButtonApiOk =
        meterOk && iconOk && ZMidnaIconSourceHook::g_orig != nullptr &&
        resolve_symbol(Z_SYM_RML_SET_CLASS, s_rmlSetClass) &&
        resolve_symbol(Z_SYM_RML_GET_PARENT, s_rmlGetParentNode) &&
        resolve_symbol(Z_SYM_RML_OWNER_DOCUMENT, s_rmlGetOwnerDocument) &&
        resolve_symbol(Z_SYM_RML_EVENT_TARGET, s_rmlEventTarget) &&
        resolve_symbol(Z_SYM_RML_CREATE_ELEMENT, s_rmlCreateElement) &&
        resolve_symbol(Z_SYM_RML_APPEND_CHILD, s_rmlAppendChild) &&
        resolve_symbol(Z_SYM_RML_GET_ELEMENT_BY_ID, s_rmlGetElementById) &&
        resolve_symbol(Z_SYM_RML_SET_ID, s_rmlSetId) &&
        resolve_symbol(Z_SYM_RML_SET_PSEUDO_CLASS, s_rmlSetPseudoClass) &&
        resolve_symbol(Z_SYM_RML_IS_PSEUDO_CLASS_SET, s_rmlIsPseudoClassSet) &&
        resolve_symbol(Z_SYM_TOUCH_EVENT_ID, s_touchEventId) &&
        mods::hook::add_pre<ZTouchDownHook>(hook_svc, before_touch_down) == MOD_OK &&
        mods::hook::add_pre<ZTouchUpHook>(hook_svc, before_touch_release) == MOD_OK &&
        mods::hook::add_pre<ZTouchCancelHook>(hook_svc, before_touch_release) == MOD_OK &&
        mods::hook::add_post<ZMobilePadReadHook>(hook_svc, after_pad_read_midna_touch) == MOD_OK &&
        mods::hook::add_pre<ZMobileMidonaAlphaHook>(hook_svc, before_midona_alpha) == MOD_OK &&
        mods::hook::add_post<ZMobileMidonaAlphaHook>(hook_svc, after_midona_alpha,
                                                     &afterTwilightHd) == MOD_OK;

    register_midna_position_vars();
    resolve_symbol(Z_SYM_GET_CONTEXT, s_getContext);
    resolve_symbol(Z_SYM_TOUCH_DOCUMENT_SIZE, s_touchDocumentSize);
    s_midnaEditorApiOk =
        s_midnaButtonApiOk && s_getContext != nullptr && s_touchDocumentSize != nullptr &&
        resolve_symbol(Z_SYM_TOUCH_DP_SCALE, s_touchDpScale) &&
        resolve_symbol(Z_SYM_TOUCH_EVENT_POSITION, s_touchEventPosition) &&
        resolve_symbol(Z_SYM_RML_ADD_EVENT_LISTENER, s_rmlAddEventListener) &&
        resolve_symbol(Z_SYM_RML_STOP_PROPAGATION, s_rmlStopPropagation) &&
        mods::hook::add_pre<ZEditorDtorHook>(hook_svc, before_editor_destroy) == MOD_OK &&
        mods::hook::add_pre<ZEditorSaveHook>(hook_svc, before_editor_save) == MOD_OK &&
        mods::hook::add_post<ZEditorResetHook>(hook_svc, after_editor_reset) == MOD_OK &&
        mods::hook::add_pre<ZDockClassesHook>(hook_svc, before_dock_classes) == MOD_OK &&
        mods::hook::add_pre<ZEditorSyncLayoutsHook>(hook_svc, before_editor_sync_layouts) ==
            MOD_OK &&
        mods::hook::add_post<ZEditorSyncLayoutsHook>(hook_svc, after_editor_sync_layouts) ==
            MOD_OK;

    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, s_midnaButtonApiOk
                                   ? "[ZButton/mobile] Midna touch button available"
                                   : "[ZButton/mobile] Midna touch button unavailable (symbols)");
        svc_log->info(mod_ctx, s_midnaEditorApiOk
                                   ? "[ZButton/mobile] Midna button in Customize Layout available"
                                   : "[ZButton/mobile] Midna button in Customize Layout unavailable");
    }
}

void z_mobile_report_after_midna_alpha(dMeter2Draw_c* draw) {
    if (!s_useCaptureFallback || draw == nullptr) {
        return;
    }
    static int s_tick = 0;
    if (++s_tick < 120) {
        return;
    }
    s_tick = 0;

    J2DScreen* screen = draw->getMainScreenPtr();
    J2DPane* midona = (screen != nullptr) ? screen->search(MULTI_CHAR('midona_n')) : nullptr;
    J2DPane* itemPane = nullptr;
    if (CPaneMgr* itemR = dMeter2Info_getMeterItemPanePtr(2)) {
        itemPane = itemR->getPanePtr();
    }

    f32 itemW = 0.0f;
    f32 itemH = 0.0f;
    if (itemPane != nullptr) {
        const JGeometry::TBox2<f32>& b = itemPane->getGlbBounds();
        itemW = b.getWidth();
        itemH = b.getHeight();
    }
}

bool z_mobile_touch_z_rect(f32& x, f32& y, f32& w, f32& h) {
    if (s_getEquipTarget == nullptr) {
        return false;
    }
    EquipTargetABI target;
    if (!s_getEquipTarget(2, target) || !target.valid) {
        return false;
    }
    if (!(target.width > 1.0f) || !(target.height > 1.0f)) {
        return false;
    }
    x = target.left;
    y = target.top;
    w = target.width;
    h = target.height;
    return true;
}

namespace {

daAlink_c* s_hbGuardLink = nullptr;
bool s_hbManualToggleOff = false;
bool s_hbWaitRelease = false;
u8 s_hbGuardFrames = 0;

constexpr u8 kBtnZ = 0x04;

bool hb_z_selected(daAlink_c* link) {
    return link != nullptr &&
           link->checkGroupItem(dItemNo_HVY_BOOTS_e, resolved_select_item(2));
}

bool hb_z_held(daAlink_c* link) {
    return link != nullptr && (link->mItemButton & kBtnZ) != 0;
}

bool hb_forced_off_context(daAlink_c* link) {
    if (link == nullptr) {
        return true;
    }
    if (link->checkWolf() || link->checkEventRun() || link->checkDeadHP() ||
        link->checkCanoeRide() || link->checkHorseRide() || link->checkBoardRide() ||
        link->checkSpinnerRide())
    {
        return true;
    }
    switch (link->mProcID) {
    case daAlink_c::PROC_DIVE_JUMP:
    case daAlink_c::PROC_SMALL_JUMP:
    case daAlink_c::PROC_CANOE_RIDE:
    case daAlink_c::PROC_CANOE_JUMP_RIDE:
    case daAlink_c::PROC_CANOE_GETOFF:
    case daAlink_c::PROC_HORSE_RIDE:
    case daAlink_c::PROC_HORSE_GETOFF:
    case daAlink_c::PROC_BOARD_RIDE:
    case daAlink_c::PROC_SPINNER_READY:
        return true;
    default:
        return false;
    }
}

void hb_clear_lock() {
    s_hbGuardLink = nullptr;
    s_hbManualToggleOff = false;
    s_hbWaitRelease = false;
    s_hbGuardFrames = 0;
}

}

bool z_mobile_hb_locked(daAlink_c* link) {
    return link != nullptr && s_hbGuardLink == link && s_hbWaitRelease;
}

void z_mobile_hb_lock(daAlink_c* link, bool manualToggleOff) {
    if (link == nullptr) {
        return;
    }
    s_hbGuardLink = link;
    s_hbManualToggleOff = manualToggleOff;
    s_hbWaitRelease = true;
    s_hbGuardFrames = manualToggleOff ? 24 : 0;
}

void z_mobile_hb_tick(daAlink_c* link) {
    if (s_hbGuardLink == nullptr || link == nullptr) {
        s_hbWaitRelease = false;
        s_hbGuardFrames = 0;
        return;
    }
    if (s_hbGuardLink != link || !s_hbWaitRelease) {
        return;
    }
    if (s_hbManualToggleOff) {
        if (s_hbGuardFrames != 0) {
            --s_hbGuardFrames;
        } else {
            hb_clear_lock();
        }
        return;
    }
    if (!hb_z_held(link)) {
        hb_clear_lock();
    }
}

HookAction z_mobile_guard_heavy_boots(void* args, void* retval) {
    if (!g_configCustomZButtonEnabled || args == nullptr) {
        return HOOK_CONTINUE;
    }
    daAlink_c* link = mods::arg<daAlink_c*>(args, 0);
    const int enable = mods::arg<int>(args, 1);

    if (link == nullptr || !link->checkEquipHeavyBoots() || link->checkNotHeavyBootsStage() ||
        !hb_z_selected(link))
    {
        return HOOK_CONTINUE;
    }

    if (enable != 0 && s_hbGuardLink == link && s_hbManualToggleOff) {
        hb_clear_lock();
        return HOOK_CONTINUE;
    }
    if (enable == 0 && s_hbGuardLink == link && s_hbManualToggleOff) {
        hb_clear_lock();
        return HOOK_CONTINUE;
    }
    if (enable != 0 && z_mobile_hb_locked(link)) {
        if (retval) *static_cast<int*>(retval) = 0;
        return HOOK_SKIP_ORIGINAL;
    }
    if (enable == 0 && hb_forced_off_context(link)) {
        hb_clear_lock();
        return HOOK_CONTINUE;
    }
    if (retval) *static_cast<int*>(retval) = 0;
    return HOOK_SKIP_ORIGINAL;
}

void z_mobile_shutdown() {
    hb_clear_lock();
    release_hosted_midona();
    s_meterButton = nullptr;
    s_meterContainer = nullptr;
    s_meterRml.clear();
    s_inDisplaySync = false;
    s_syncZButton = nullptr;
    set_midna_button_shown(false);
    s_midnaTouchPressed = false;
    s_midnaTouchTrig = false;
}

#else

bool z_mobile_active() {
    return false;
}
bool z_mobile_consume_midna_touch() {
    return false;
}
bool z_mobile_twilight_hd_touch_z() {
    return false;
}
bool z_mobile_twilight_hd_z_is_item() {
    return false;
}
void z_mobile_init(const HookService*) {}
bool z_mobile_wants_midona_host() {
    return false;
}
J2DPane* z_mobile_sync_touch_z(dMeter2Draw_c*) {
    return nullptr;
}
void z_mobile_report_after_midna_alpha(dMeter2Draw_c*) {}
bool z_mobile_touch_z_rect(f32&, f32&, f32&, f32&) {
    return false;
}
bool z_mobile_hb_locked(daAlink_c*) {
    return false;
}
void z_mobile_hb_lock(daAlink_c*, bool) {}
void z_mobile_hb_tick(daAlink_c*) {}
HookAction z_mobile_guard_heavy_boots(void*, void*) {
    return HOOK_CONTINUE;
}
void z_mobile_shutdown() {}

#endif

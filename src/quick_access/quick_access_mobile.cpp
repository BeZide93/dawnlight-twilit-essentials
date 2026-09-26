#include "quick_access_mobile.hpp"

#if Z_MOBILE_BUILD

#include "quick_access.hpp"
#include "../controls/controls.hpp"

#include <string>

extern const LogService* svc_log;

#define QA_SYM_SYNC_DISPLAYS "_ZN4dusk2ui13TouchControls21sync_control_displaysEv"
#define QA_SYM_RML_SET_CLASS                                                                      \
    "_ZN3Rml7Element8SetClassERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb"
#define QA_SYM_RML_GET_ELEMENT_BY_ID                                                              \
    "_ZN3Rml7Element14GetElementByIdERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_RML_GET_CHILD "_ZNK3Rml7Element8GetChildEi"
#define QA_SYM_RML_GET_PARENT "_ZNK3Rml7Element13GetParentNodeEv"
#define QA_SYM_RML_OWNER_DOCUMENT "_ZNK3Rml7Element16GetOwnerDocumentEv"
#define QA_SYM_RML_EVENT_TARGET "_ZNK3Rml5Event16GetTargetElementEv"
#define QA_SYM_RML_CREATE_ELEMENT                                                                 \
    "_ZN3Rml15ElementDocument13CreateElementERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_RML_APPEND_CHILD                                                                   \
    "_ZN3Rml7Element11AppendChildENSt6__ndk110unique_ptrIS0_NS_8ReleaserIS0_EEEEb"
#define QA_SYM_RML_SET_ID                                                                         \
    "_ZN3Rml7Element5SetIdERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_RML_SET_PROPERTY                                                                   \
    "_ZN3Rml7Element11SetPropertyERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_"
#define QA_SYM_RML_SET_INNER_RML                                                                  \
    "_ZN3Rml7Element11SetInnerRMLERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_RML_SET_PSEUDO_CLASS                                                               \
    "_ZN3Rml7Element14SetPseudoClassERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb"
#define QA_SYM_RML_IS_PSEUDO_CLASS_SET                                                            \
    "_ZNK3Rml7Element16IsPseudoClassSetERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_TOUCH_DOWN "_ZN4dusk2ui13TouchControls17handle_touch_downERN3Rml5EventE"
#define QA_SYM_TOUCH_UP "_ZN4dusk2ui13TouchControls15handle_touch_upERN3Rml5EventE"
#define QA_SYM_TOUCH_CANCEL "_ZN4dusk2ui13TouchControls19handle_touch_cancelERN3Rml5EventE"
#define QA_SYM_TOUCH_EVENT_ID "_ZN4dusk2ui14touch_event_idERKN3Rml5EventE"

DEFINE_HOOK_SYMBOL(QA_SYM_SYNC_DISPLAYS, void(void*), QaSyncDisplaysHook);
DEFINE_HOOK_SYMBOL(QA_SYM_RML_SET_CLASS, void(void*, const std::string*, bool), QaRmlSetClassHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_DOWN, void(void*, void*), QaTouchDownHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_UP, void(void*, void*), QaTouchUpHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_CANCEL, void(void*, void*), QaTouchCancelHook);
DEFINE_HOOK(&mDoCPd_c::read, QaPadReadHook);

namespace {

using RmlGetElementByIdFn = void* (*)(void*, const std::string*);
using RmlGetChildFn = void* (*)(const void*, int);
using RmlGetNodeFn = void* (*)(const void*);
using RmlSetIdFn = void (*)(void*, const std::string*);
using RmlSetClassFn = void (*)(void*, const std::string*, bool);
using RmlSetPropertyFn = bool (*)(void*, const std::string*, const std::string*);
using RmlSetInnerRMLFn = void (*)(void*, const std::string*);
using RmlSetPseudoClassFn = void (*)(void*, const std::string*, bool);
using RmlIsPseudoClassSetFn = bool (*)(const void*, const std::string*);
using TouchEventIdFn = uint64_t (*)(const void*);

struct RmlElementPtr {
    void* element = nullptr;
    ~RmlElementPtr() {}
};
using RmlCreateElementFn = RmlElementPtr (*)(void*, const std::string*);
using RmlAppendChildFn = void* (*)(void*, RmlElementPtr*, bool);

const HookService* s_hookSvc = nullptr;
bool s_apiOk = false;

RmlGetElementByIdFn s_rmlGetElementById = nullptr;
RmlGetChildFn s_rmlGetChild = nullptr;
RmlGetNodeFn s_rmlGetParentNode = nullptr;
RmlGetNodeFn s_rmlGetOwnerDocument = nullptr;
RmlGetNodeFn s_rmlEventTarget = nullptr;
RmlSetIdFn s_rmlSetId = nullptr;
RmlSetClassFn s_rmlSetClass = nullptr;
RmlSetPropertyFn s_rmlSetProperty = nullptr;
RmlSetInnerRMLFn s_rmlSetInnerRML = nullptr;
RmlSetPseudoClassFn s_rmlSetPseudoClass = nullptr;
RmlIsPseudoClassSetFn s_rmlIsPseudoClassSet = nullptr;
TouchEventIdFn s_touchEventId = nullptr;
RmlCreateElementFn s_rmlCreateElement = nullptr;
RmlAppendChildFn s_rmlAppendChild = nullptr;

constexpr const char* kButtonId = "twe-quick-access-touch";
constexpr const char* kQuickAccessGlyph = "&#xe5c3;";

void* s_button = nullptr;
void* s_icon = nullptr;

bool s_buttonShown = false;
bool s_touchActive = false;
uint64_t s_touchFinger = 0;
bool s_held = false;

bool s_inDisplaySync = false;
void* s_syncAnchor = nullptr;

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

void set_property(void* element, const char* name, const char* value) {
    if (element == nullptr || s_rmlSetProperty == nullptr) {
        return;
    }
    const std::string propertyName = name;
    const std::string propertyValue = value;
    s_rmlSetProperty(element, &propertyName, &propertyValue);
}

constexpr const char* kIconOpacityIdle = "0.55";
constexpr const char* kIconOpacityPressed = "1";

void set_button_pressed(bool pressed) {
    if (s_button == nullptr) {
        return;
    }
    const std::string pressedClass = "pressed";
    s_rmlSetClass(s_button, &pressedClass, pressed);
    set_property(s_icon, "opacity", pressed ? kIconOpacityPressed : kIconOpacityIdle);
}

void clear_held() {
    s_held = false;
    if (s_touchActive) {
        s_touchActive = false;
        set_button_pressed(false);
    }
}

void set_button_shown(bool shown) {
    if (s_button == nullptr || shown == s_buttonShown) {
        return;
    }
    const std::string hiddenClass = "hidden";
    s_rmlSetPseudoClass(s_button, &hiddenClass, !shown);
    s_buttonShown = shown;
    if (!shown) {
        clear_held();
    }
}

void* create_button(void* sibling, void* parent) {
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

    const std::string id = kButtonId;
    s_rmlSetId(button, &id);
    const std::string className = "control";
    s_rmlSetClass(button, &className, true);
    set_property(button, "width", "64dp");
    set_property(button, "height", "46dp");
    set_property(button, "left", "298dp");
    set_property(button, "bottom", "0dp");
    set_property(button, "border-radius", "23dp");

    const std::string rml = std::string("<icon><glyph>") + kQuickAccessGlyph + "</glyph></icon>";
    s_rmlSetInnerRML(button, &rml);
    return button;
}

bool quick_access_wanted() {
    return g_configQuickAccessEnabled && !isTitleOrMainMenu() && !is_pause_menu_open();
}

void sync_button(void* actionBar) {
    if (!s_apiOk) {
        return;
    }

    const std::string hiddenClass = "hidden";
    const bool actionBarShown =
        actionBar != nullptr && !s_rmlIsPseudoClassSet(actionBar, &hiddenClass);
    const bool wanted = actionBarShown && quick_access_wanted();
    if (!wanted) {
        set_button_shown(false);
        return;
    }

    void* parent = s_rmlGetParentNode(actionBar);
    if (parent == nullptr) {
        set_button_shown(false);
        return;
    }
    const std::string id = kButtonId;
    void* button = s_rmlGetElementById(parent, &id);
    if (button == nullptr) {
        button = create_button(actionBar, parent);
        if (button == nullptr) {
            return;
        }
    }
    if (button != s_button) {
        s_button = button;
        s_buttonShown = false;
        s_icon = s_rmlGetChild != nullptr ? s_rmlGetChild(button, 0) : nullptr;
        set_property(s_icon, "opacity", s_touchActive ? kIconOpacityPressed : kIconOpacityIdle);
    }
    set_button_shown(true);
}

bool event_hits_button(void* event) {
    if (!s_buttonShown || s_button == nullptr || event == nullptr) {
        return false;
    }
    void* element = s_rmlEventTarget(event);
    for (int depth = 0; element != nullptr && depth < 4; ++depth) {
        if (element == s_button) {
            return true;
        }
        element = s_rmlGetParentNode(element);
    }
    return false;
}

HookAction before_touch_down(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    if (!event_hits_button(event)) {
        return HOOK_CONTINUE;
    }
    if (!s_touchActive) {
        s_touchActive = true;
        s_touchFinger = s_touchEventId(event);
        s_held = true;
        set_button_pressed(true);
    }
    return HOOK_SKIP_ORIGINAL;
}

HookAction before_touch_release(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    if (!s_touchActive || event == nullptr || s_touchEventId(event) != s_touchFinger) {
        return HOOK_CONTINUE;
    }
    s_touchActive = false;
    s_held = false;
    set_button_pressed(false);
    return HOOK_SKIP_ORIGINAL;
}

void after_pad_read(ModContext*, void*, void*, void*) {
    if (!s_held) {
        return;
    }
    const u32 bit = controls_binding_bit(CTRL_BIND_QUICK_ACCESS);
    if (bit == 0) {
        return;
    }
    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    if ((pad.mButtonFlags & bit) == 0) {
        pad.mPressedButtonFlags |= bit;
    }
    pad.mButtonFlags |= bit;
}

HookAction before_sync_displays(ModContext*, void*, void*, void*) {
    s_inDisplaySync = true;
    s_syncAnchor = nullptr;
    return HOOK_CONTINUE;
}

void after_sync_displays(ModContext*, void*, void*, void*) {
    s_inDisplaySync = false;
    void* anchor = s_syncAnchor;
    s_syncAnchor = nullptr;
    if (anchor == nullptr) {
        set_button_shown(false);
        return;
    }
    void* root = s_rmlGetParentNode(anchor);
    if (root == nullptr) {
        set_button_shown(false);
        return;
    }
    const std::string id = "action-bar";
    void* actionBar = s_rmlGetElementById(root, &id);
    sync_button(actionBar);
}

HookAction before_rml_set_class(ModContext*, void* args, void*, void*) {
    if (!s_inDisplaySync || s_syncAnchor != nullptr || args == nullptr) {
        return HOOK_CONTINUE;
    }
    void* element = mods::arg<void*>(args, 0);
    const std::string* className = mods::arg<const std::string*>(args, 1);
    if (element != nullptr && className != nullptr && *className == "has-icon") {
        s_syncAnchor = element;
    }
    return HOOK_CONTINUE;
}

}

void quick_access_mobile_init(const HookService* hook_svc) {
    s_hookSvc = hook_svc;
    if (hook_svc == nullptr) {
        return;
    }

    s_apiOk = resolve_symbol(QA_SYM_RML_GET_ELEMENT_BY_ID, s_rmlGetElementById) &&
              resolve_symbol(QA_SYM_RML_GET_CHILD, s_rmlGetChild) &&
              resolve_symbol(QA_SYM_RML_GET_PARENT, s_rmlGetParentNode) &&
              resolve_symbol(QA_SYM_RML_OWNER_DOCUMENT, s_rmlGetOwnerDocument) &&
              resolve_symbol(QA_SYM_RML_EVENT_TARGET, s_rmlEventTarget) &&
              resolve_symbol(QA_SYM_RML_SET_ID, s_rmlSetId) &&
              resolve_symbol(QA_SYM_RML_SET_CLASS, s_rmlSetClass) &&
              resolve_symbol(QA_SYM_RML_SET_PROPERTY, s_rmlSetProperty) &&
              resolve_symbol(QA_SYM_RML_SET_INNER_RML, s_rmlSetInnerRML) &&
              resolve_symbol(QA_SYM_RML_SET_PSEUDO_CLASS, s_rmlSetPseudoClass) &&
              resolve_symbol(QA_SYM_RML_IS_PSEUDO_CLASS_SET, s_rmlIsPseudoClassSet) &&
              resolve_symbol(QA_SYM_TOUCH_EVENT_ID, s_touchEventId) &&
              resolve_symbol(QA_SYM_RML_CREATE_ELEMENT, s_rmlCreateElement) &&
              resolve_symbol(QA_SYM_RML_APPEND_CHILD, s_rmlAppendChild) &&
              mods::hook::add_pre<QaRmlSetClassHook>(hook_svc, before_rml_set_class) == MOD_OK &&
              mods::hook::add_pre<QaSyncDisplaysHook>(hook_svc, before_sync_displays) == MOD_OK &&
              mods::hook::add_post<QaSyncDisplaysHook>(hook_svc, after_sync_displays) == MOD_OK &&
              mods::hook::add_pre<QaTouchDownHook>(hook_svc, before_touch_down) == MOD_OK &&
              mods::hook::add_pre<QaTouchUpHook>(hook_svc, before_touch_release) == MOD_OK &&
              mods::hook::add_pre<QaTouchCancelHook>(hook_svc, before_touch_release) == MOD_OK;

    HookOptions padReadOptions = HOOK_OPTIONS_INIT;
    padReadOptions.priority = 200;
    mods::hook::add_post<QaPadReadHook>(hook_svc, after_pad_read, &padReadOptions);

    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, s_apiOk ? "[QuickAccess/mobile] touch button available"
                                       : "[QuickAccess/mobile] touch button unavailable (symbols)");
    }
}

void quick_access_mobile_shutdown() {
    clear_held();
    s_button = nullptr;
    s_icon = nullptr;
    s_buttonShown = false;
}

#else

void quick_access_mobile_init(const HookService*) {}
void quick_access_mobile_shutdown() {}

#endif

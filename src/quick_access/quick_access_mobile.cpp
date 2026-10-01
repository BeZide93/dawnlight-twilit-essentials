#include "quick_access_mobile.hpp"

#if Z_MOBILE_BUILD

#include "quick_access.hpp"
#include "quick_access_bottles.hpp"
#include "../controls/controls.hpp"
#include "../boss_rush/boss_rush.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

extern const LogService* svc_log;
extern const ConfigService* svc_config;

#define QA_SYM_SYNC_DISPLAYS "_ZN4dusk2ui13TouchControls21sync_control_displaysEv"
#define QA_SYM_RML_SET_CLASS                                                                      \
    "_ZN3Rml7Element8SetClassERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEb"
#define QA_SYM_RML_GET_ELEMENT_BY_ID                                                              \
    "_ZN3Rml7Element14GetElementByIdERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE"
#define QA_SYM_RML_GET_PARENT "_ZNK3Rml7Element13GetParentNodeEv"
#define QA_SYM_RML_OWNER_DOCUMENT "_ZNK3Rml7Element16GetOwnerDocumentEv"
#define QA_SYM_RML_GET_CONTEXT "_ZNK3Rml7Element10GetContextEv"
#define QA_SYM_RML_EVENT_TARGET "_ZNK3Rml5Event16GetTargetElementEv"
#define QA_SYM_RML_STOP_PROPAGATION "_ZN3Rml5Event15StopPropagationEv"
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
#define QA_SYM_RML_ADD_LISTENER                                                                   \
    "_ZN3Rml7Element16AddEventListenerERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEPNS_13EventListenerEb"
#define QA_SYM_TOUCH_DOWN "_ZN4dusk2ui13TouchControls17handle_touch_downERN3Rml5EventE"
#define QA_SYM_TOUCH_UP "_ZN4dusk2ui13TouchControls15handle_touch_upERN3Rml5EventE"
#define QA_SYM_TOUCH_CANCEL "_ZN4dusk2ui13TouchControls19handle_touch_cancelERN3Rml5EventE"
#define QA_SYM_TOUCH_EVENT_ID "_ZN4dusk2ui14touch_event_idERKN3Rml5EventE"
#define QA_SYM_TOUCH_EVENT_POSITION "_ZN4dusk2ui20touch_event_positionERKN3Rml5EventE"
#define QA_SYM_TOUCH_DOC_SIZE "_ZN4dusk2ui22touch_document_size_dpEPN3Rml7ContextE"
#define QA_SYM_TOUCH_DP_SCALE "_ZN4dusk2ui14touch_dp_scaleEPN3Rml7ContextE"
#define QA_SYM_EDITOR_SYNC_LAYOUTS "_ZN4dusk2ui19TouchControlsEditor20sync_control_layoutsEv"
#define QA_SYM_EDITOR_SAVE "_ZN4dusk2ui19TouchControlsEditor11save_layoutEv"
#define QA_SYM_EDITOR_RESET "_ZN4dusk2ui19TouchControlsEditor20reset_working_layoutEv"
#define QA_SYM_EDITOR_DTOR "_ZN4dusk2ui19TouchControlsEditorD2Ev"

DEFINE_HOOK_SYMBOL(QA_SYM_SYNC_DISPLAYS, void(void*), QaSyncDisplaysHook);
DEFINE_HOOK_SYMBOL(QA_SYM_RML_SET_CLASS, void(void*, const std::string*, bool), QaRmlSetClassHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_DOWN, void(void*, void*), QaTouchDownHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_UP, void(void*, void*), QaTouchUpHook);
DEFINE_HOOK_SYMBOL(QA_SYM_TOUCH_CANCEL, void(void*, void*), QaTouchCancelHook);
DEFINE_HOOK_SYMBOL(QA_SYM_EDITOR_SYNC_LAYOUTS, void(void*), QaEditorSyncLayoutsHook);
DEFINE_HOOK_SYMBOL(QA_SYM_EDITOR_SAVE, void(void*), QaEditorSaveHook);
DEFINE_HOOK_SYMBOL(QA_SYM_EDITOR_RESET, void(void*), QaEditorResetHook);
DEFINE_HOOK_SYMBOL(QA_SYM_EDITOR_DTOR, void(void*), QaEditorDtorHook);
DEFINE_HOOK(&mDoCPd_c::read, QaPadReadHook);

namespace {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

using RmlGetElementByIdFn = void* (*)(void*, const std::string*);
using RmlGetNodeFn = void* (*)(const void*);
using RmlSetIdFn = void (*)(void*, const std::string*);
using RmlSetClassFn = void (*)(void*, const std::string*, bool);
using RmlSetPropertyFn = bool (*)(void*, const std::string*, const std::string*);
using RmlSetInnerRMLFn = void (*)(void*, const std::string*);
using RmlSetPseudoClassFn = void (*)(void*, const std::string*, bool);
using RmlIsPseudoClassSetFn = bool (*)(const void*, const std::string*);
using RmlAddListenerFn = void (*)(void*, const std::string*, void*, bool);
using RmlStopPropagationFn = void (*)(void*);
using TouchEventIdFn = uint64_t (*)(const void*);
using TouchEventPositionFn = Vec2 (*)(const void*);
using TouchDocSizeFn = Vec2 (*)(void*);
using TouchDpScaleFn = float (*)(void*);

struct RmlElementPtr {
    void* element = nullptr;
    ~RmlElementPtr() {}
};
using RmlCreateElementFn = RmlElementPtr (*)(void*, const std::string*);
using RmlAppendChildFn = void* (*)(void*, RmlElementPtr*, bool);

const HookService* s_hookSvc = nullptr;
bool s_apiOk = false;
bool s_editorApiOk = false;

RmlGetElementByIdFn s_rmlGetElementById = nullptr;
RmlGetNodeFn s_rmlGetParentNode = nullptr;
RmlGetNodeFn s_rmlGetOwnerDocument = nullptr;
RmlGetNodeFn s_rmlGetContext = nullptr;
RmlGetNodeFn s_rmlEventTarget = nullptr;
RmlSetIdFn s_rmlSetId = nullptr;
RmlSetClassFn s_rmlSetClass = nullptr;
RmlSetPropertyFn s_rmlSetProperty = nullptr;
RmlSetInnerRMLFn s_rmlSetInnerRML = nullptr;
RmlSetPseudoClassFn s_rmlSetPseudoClass = nullptr;
RmlIsPseudoClassSetFn s_rmlIsPseudoClassSet = nullptr;
RmlAddListenerFn s_rmlAddListener = nullptr;
RmlStopPropagationFn s_rmlStopPropagation = nullptr;
TouchEventIdFn s_touchEventId = nullptr;
TouchEventPositionFn s_touchEventPosition = nullptr;
TouchDocSizeFn s_touchDocSize = nullptr;
TouchDpScaleFn s_touchDpScale = nullptr;
RmlCreateElementFn s_rmlCreateElement = nullptr;
RmlAppendChildFn s_rmlAppendChild = nullptr;

constexpr const char* kBarId = "twe-quick-bar";
constexpr const char* kQaButtonId = "twe-quick-bar-qa";
constexpr const char* kBottleButtonId = "twe-quick-bar-bottles";
constexpr const char* kSeparatorId = "twe-quick-bar-sep";
constexpr const char* kFrameId = "twe-quick-bar-frame";
constexpr const char* kBarInnerRml =
    "<button id=\"twe-quick-bar-qa\" class=\"utility\"><icon><glyph>&#xe5c3;</glyph></icon></button>"
    "<separator id=\"twe-quick-bar-sep\" />"
    "<button id=\"twe-quick-bar-bottles\" class=\"utility\"><icon><glyph>&#xf69d;</glyph></icon></button>";

enum Anchor : int {
    ANCHOR_NONE,
    ANCHOR_TOP,
    ANCHOR_LEFT,
    ANCHOR_BOTTOM,
    ANCHOR_RIGHT,
    ANCHOR_TOP_LEFT,
    ANCHOR_TOP_RIGHT,
    ANCHOR_BOTTOM_LEFT,
    ANCHOR_BOTTOM_RIGHT,
};

struct Props {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float scale = 1.0f;
    int anchor = ANCHOR_NONE;
};

struct Rect {
    float l = 0.0f;
    float t = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Layout {
    Rect visual;
    Rect box;
    float scale = 1.0f;
};

constexpr Props kDefaultProps = {298.0f, 0.0f, 113.0f, 46.0f, 1.0f, ANCHOR_BOTTOM_LEFT};
constexpr float kDragThresholdDp = 6.0f;
constexpr float kMinScale = 0.25f;
constexpr float kMinBarWidthDp = 56.0f;
constexpr float kMinBarHeightDp = 36.0f;

Props s_savedProps = kDefaultProps;
ConfigVarHandle s_layoutVar = 0;

enum Handle : int {
    HANDLE_MOVE,
    HANDLE_LEFT,
    HANDLE_RIGHT,
    HANDLE_TOP,
    HANDLE_BOTTOM,
    HANDLE_TOP_LEFT,
    HANDLE_TOP_RIGHT,
    HANDLE_BOTTOM_LEFT,
    HANDLE_BOTTOM_RIGHT,
    HANDLE_ROOT,
    HANDLE_ENGINE,
    HANDLE_COUNT,
};

constexpr const char* kHandleRml =
    "<resize-handle id=\"twe-quick-bar-h1\" class=\"edge horizontal left\" />"
    "<resize-handle id=\"twe-quick-bar-h2\" class=\"edge horizontal right\" />"
    "<resize-handle id=\"twe-quick-bar-h3\" class=\"edge vertical top\" />"
    "<resize-handle id=\"twe-quick-bar-h4\" class=\"edge vertical bottom\" />"
    "<resize-handle id=\"twe-quick-bar-h5\" class=\"corner top left\" />"
    "<resize-handle id=\"twe-quick-bar-h6\" class=\"corner top right\" />"
    "<resize-handle id=\"twe-quick-bar-h7\" class=\"corner bottom left\" />"
    "<resize-handle id=\"twe-quick-bar-h8\" class=\"corner bottom right\" />";

enum EventKind : int {
    EV_START,
    EV_MOVE,
    EV_END,
    EV_CANCEL,
    EV_COUNT,
};

constexpr const char* kEventNames[EV_COUNT] = {"touchstart", "touchmove", "touchend", "touchcancel"};

struct Applied {
    void* element = nullptr;
    Rect box;
    float scale = 0.0f;
    int dock = -1;
    int shownMask = -1;
};

struct BarElements {
    void* bar = nullptr;
    void* qa = nullptr;
    void* bottles = nullptr;
    void* separator = nullptr;
    Applied applied;
};

BarElements s_game;
bool s_barShown = false;
bool s_touchActive = false;
uint64_t s_touchFinger = 0;
bool s_qaHeld = false;
bool s_bottlesHeld = false;
void* s_pressedButton = nullptr;

bool s_inDisplaySync = false;
void* s_syncAnchor = nullptr;

bool s_inEditorSync = false;
void* s_editorAnchor = nullptr;
void* s_editorRoot = nullptr;
void* s_engineFrame = nullptr;
void* s_editorFrame = nullptr;
Applied s_editorFrameApplied;
BarElements s_editor;
Props s_editProps = kDefaultProps;
bool s_editorSelected = false;

struct PointerEdit {
    bool active = false;
    bool dragging = false;
    int handle = HANDLE_MOVE;
    uint64_t finger = 0;
    Vec2 startPointer;
    Rect startVisual;
    Props startProps;
};
PointerEdit s_edit;

void editor_event(int handle, int kind, void* event);

class EditorListener {
public:
    virtual ~EditorListener() {}
    virtual void ProcessEvent(void* event) { editor_event(handle, kind, event); }
    virtual void OnAttach(void*) {}
    virtual void OnDetach(void*) {}

    void* mObserverBlock = nullptr;
    int handle = HANDLE_MOVE;
    int kind = EV_START;
};

constexpr const char* kEngineControlIds[] = {
    "trigger-l", "trigger-r", "button-z", "action-bar", "skip",
    "button-y",  "button-x",  "button-b", "button-a",
};

bool s_forcingClass = false;

EditorListener s_listeners[HANDLE_COUNT][EV_COUNT];

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

void set_class(void* element, const char* name, bool on) {
    if (element == nullptr) {
        return;
    }
    const std::string className = name;
    s_rmlSetClass(element, &className, on);
}

void* find_by_id(void* root, const char* id) {
    if (root == nullptr) {
        return nullptr;
    }
    const std::string str = id;
    return s_rmlGetElementById(root, &str);
}

bool near(float a, float b) {
    return std::fabs(a - b) <= 0.01f;
}

bool rect_near(const Rect& a, const Rect& b) {
    return near(a.l, b.l) && near(a.t, b.t) && near(a.w, b.w) && near(a.h, b.h);
}

Rect resolve_anchored_rect(int anchor, float x, float y, float w, float h, Vec2 doc) {
    switch (anchor) {
    case ANCHOR_TOP: return {x * doc.x - w * 0.5f, y, w, h};
    case ANCHOR_BOTTOM: return {x * doc.x - w * 0.5f, doc.y - y - h, w, h};
    case ANCHOR_LEFT: return {x, y * doc.y - h * 0.5f, w, h};
    case ANCHOR_RIGHT: return {doc.x - x - w, y * doc.y - h * 0.5f, w, h};
    case ANCHOR_TOP_LEFT: return {x, y, w, h};
    case ANCHOR_TOP_RIGHT: return {doc.x - x - w, y, w, h};
    case ANCHOR_BOTTOM_LEFT: return {x, doc.y - y - h, w, h};
    case ANCHOR_BOTTOM_RIGHT: return {doc.x - x - w, doc.y - y - h, w, h};
    default: return {x * doc.x - w * 0.5f, y * doc.y - h * 0.5f, w, h};
    }
}

Layout resolve_layout(const Props& props, Vec2 doc) {
    Layout layout;
    layout.visual = resolve_anchored_rect(props.anchor, props.x, props.y, props.w * props.scale,
                                          props.h * props.scale, doc);
    layout.box = {layout.visual.l + (layout.visual.w - props.w) * 0.5f,
                  layout.visual.t + (layout.visual.h - props.h) * 0.5f, props.w, props.h};
    layout.scale = props.scale;
    return layout;
}

int dock_anchor(const Rect& v, Vec2 doc) {
    if (doc.x <= 0.0f || doc.y <= 0.0f || v.w <= 0.0f || v.h <= 0.0f) {
        return ANCHOR_NONE;
    }
    const bool top = near(v.t, 0.0f);
    const bool bottom = near(v.t + v.h, doc.y);
    const bool left = near(v.l, 0.0f);
    const bool right = near(v.l + v.w, doc.x);
    if (top && left && !right) return ANCHOR_TOP_LEFT;
    if (top && right && !left) return ANCHOR_TOP_RIGHT;
    if (bottom && left && !right) return ANCHOR_BOTTOM_LEFT;
    if (bottom && right && !left) return ANCHOR_BOTTOM_RIGHT;
    if (top) return ANCHOR_TOP;
    if (bottom) return ANCHOR_BOTTOM;
    if (left) return ANCHOR_LEFT;
    if (right) return ANCHOR_RIGHT;
    return ANCHOR_NONE;
}

Props encode_props(const Rect& v, Vec2 doc, Props props, int anchor) {
    props.anchor = anchor;
    switch (anchor) {
    case ANCHOR_TOP:
        props.x = (v.l + v.w * 0.5f) / doc.x;
        props.y = v.t;
        break;
    case ANCHOR_BOTTOM:
        props.x = (v.l + v.w * 0.5f) / doc.x;
        props.y = doc.y - v.t - v.h;
        break;
    case ANCHOR_LEFT:
        props.x = v.l;
        props.y = (v.t + v.h * 0.5f) / doc.y;
        break;
    case ANCHOR_RIGHT:
        props.x = doc.x - v.l - v.w;
        props.y = (v.t + v.h * 0.5f) / doc.y;
        break;
    case ANCHOR_TOP_LEFT:
        props.x = v.l;
        props.y = v.t;
        break;
    case ANCHOR_TOP_RIGHT:
        props.x = doc.x - v.l - v.w;
        props.y = v.t;
        break;
    case ANCHOR_BOTTOM_LEFT:
        props.x = v.l;
        props.y = doc.y - v.t - v.h;
        break;
    case ANCHOR_BOTTOM_RIGHT:
        props.x = doc.x - v.l - v.w;
        props.y = doc.y - v.t - v.h;
        break;
    default:
        props.x = (v.l + v.w * 0.5f) / doc.x;
        props.y = (v.t + v.h * 0.5f) / doc.y;
        break;
    }
    return props;
}

Vec2 doc_size(void* element) {
    if (element == nullptr || s_rmlGetContext == nullptr || s_touchDocSize == nullptr) {
        return {};
    }
    void* context = s_rmlGetContext(element);
    if (context == nullptr) {
        return {};
    }
    return s_touchDocSize(context);
}

float dp_scale(void* element) {
    if (element == nullptr || s_rmlGetContext == nullptr || s_touchDpScale == nullptr) {
        return 1.0f;
    }
    void* context = s_rmlGetContext(element);
    return context != nullptr ? s_touchDpScale(context) : 1.0f;
}

void set_box(void* element, const Rect& box) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3fdp", box.l);
    set_property(element, "left", buf);
    std::snprintf(buf, sizeof(buf), "%.3fdp", box.t);
    set_property(element, "top", buf);
    std::snprintf(buf, sizeof(buf), "%.3fdp", box.w);
    set_property(element, "width", buf);
    std::snprintf(buf, sizeof(buf), "%.3fdp", box.h);
    set_property(element, "height", buf);
}

void apply_dock_classes(void* element, int anchor) {
    const bool top = anchor == ANCHOR_TOP || anchor == ANCHOR_TOP_LEFT || anchor == ANCHOR_TOP_RIGHT;
    const bool bottom =
        anchor == ANCHOR_BOTTOM || anchor == ANCHOR_BOTTOM_LEFT || anchor == ANCHOR_BOTTOM_RIGHT;
    const bool left = anchor == ANCHOR_LEFT || anchor == ANCHOR_TOP_LEFT || anchor == ANCHOR_BOTTOM_LEFT;
    const bool right =
        anchor == ANCHOR_RIGHT || anchor == ANCHOR_TOP_RIGHT || anchor == ANCHOR_BOTTOM_RIGHT;
    set_class(element, "docked", top || bottom || left || right);
    set_class(element, "docked-top", top);
    set_class(element, "docked-bottom", bottom);
    set_class(element, "docked-left", left);
    set_class(element, "docked-right", right);
}

bool apply_layout(BarElements& els, const Props& props) {
    const Vec2 doc = doc_size(els.bar);
    if (doc.x <= 0.0f || doc.y <= 0.0f) {
        return false;
    }
    const Layout layout = resolve_layout(props, doc);
    const int dock = dock_anchor(layout.visual, doc);
    Applied& a = els.applied;
    if (a.element == els.bar && rect_near(a.box, layout.box) && near(a.scale, layout.scale) &&
        a.dock == dock) {
        return true;
    }
    set_box(els.bar, layout.box);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "scale(%.4f)", layout.scale);
    set_property(els.bar, "transform", buf);
    apply_dock_classes(els.bar, dock);
    a.element = els.bar;
    a.box = layout.box;
    a.scale = layout.scale;
    a.dock = dock;
    return true;
}

void apply_button_visibility(BarElements& els, bool qa) {
    const int mask = qa ? 1 : 0;
    if (els.applied.shownMask == mask) {
        return;
    }
    set_property(els.qa, "display", qa ? "flex" : "none");
    set_property(els.bottles, "display", qa ? "flex" : "none");
    set_property(els.separator, "display", qa ? "block" : "none");
    els.applied.shownMask = mask;
}

bool fetch_children(BarElements& els) {
    els.qa = find_by_id(els.bar, kQaButtonId);
    els.bottles = find_by_id(els.bar, kBottleButtonId);
    els.separator = find_by_id(els.bar, kSeparatorId);
    els.applied = {};
    return els.qa != nullptr && els.bottles != nullptr;
}

void* create_element(void* sibling, void* parent, const char* tag, const char* id,
                     const char* className, const char* innerRml) {
    void* document = s_rmlGetOwnerDocument(sibling);
    if (document == nullptr) {
        return nullptr;
    }
    const std::string tagName = tag;
    RmlElementPtr created = s_rmlCreateElement(document, &tagName);
    if (created.element == nullptr) {
        return nullptr;
    }
    void* element = s_rmlAppendChild(parent, &created, true);
    if (element == nullptr) {
        return nullptr;
    }
    const std::string idStr = id;
    s_rmlSetId(element, &idStr);
    if (className != nullptr) {
        set_class(element, className, true);
    }
    if (innerRml != nullptr) {
        const std::string rml = innerRml;
        s_rmlSetInnerRML(element, &rml);
    }
    return element;
}

void load_saved_props() {
    s_savedProps = kDefaultProps;
    if (svc_config == nullptr || s_layoutVar == 0) {
        return;
    }
    char buf[128] = {};
    size_t len = 0;
    if (svc_config->get_string(mod_ctx, s_layoutVar, buf, sizeof(buf), &len) != MOD_OK) {
        return;
    }
    Props p;
    if (std::sscanf(buf, "%f,%f,%f,%f,%f,%d", &p.x, &p.y, &p.w, &p.h, &p.scale, &p.anchor) == 6 &&
        p.w > 0.0f && p.h > 0.0f && p.scale >= kMinScale && p.anchor >= ANCHOR_NONE &&
        p.anchor <= ANCHOR_BOTTOM_RIGHT) {
        s_savedProps = p;
    }
}

void store_saved_props() {
    if (svc_config == nullptr || s_layoutVar == 0) {
        return;
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%.4f,%.4f,%d", s_savedProps.x, s_savedProps.y,
                  s_savedProps.w, s_savedProps.h, s_savedProps.scale, s_savedProps.anchor);
    svc_config->set_string(mod_ctx, s_layoutVar, buf);
}

bool common_wanted() {
    return !isTitleOrMainMenu() && !is_pause_menu_open() && !is_boss_rush_active();
}

bool qa_wanted() {
    return quick_access_enabled() && !controls_binding_blocked(CTRL_BIND_QUICK_ACCESS);
}

void release_touch() {
    s_qaHeld = false;
    s_bottlesHeld = false;
    if (s_pressedButton != nullptr) {
        set_class(s_pressedButton, "pressed", false);
        s_pressedButton = nullptr;
    }
    s_touchActive = false;
}

void set_bar_shown(bool shown) {
    if (s_game.bar == nullptr || shown == s_barShown) {
        return;
    }
    const std::string hiddenClass = "hidden";
    s_rmlSetPseudoClass(s_game.bar, &hiddenClass, !shown);
    s_barShown = shown;
    if (!shown) {
        release_touch();
    }
}

void sync_game_bar(void* actionBar) {
    if (!s_apiOk) {
        return;
    }

    const std::string hiddenClass = "hidden";
    const bool actionBarShown =
        actionBar != nullptr && !s_rmlIsPseudoClassSet(actionBar, &hiddenClass);
    const bool qa = qa_wanted();
    if (!actionBarShown || !common_wanted() || !qa) {
        set_bar_shown(false);
        return;
    }

    void* parent = s_rmlGetParentNode(actionBar);
    if (parent == nullptr) {
        set_bar_shown(false);
        return;
    }
    void* bar = find_by_id(parent, kBarId);
    if (bar == nullptr) {
        bar = create_element(actionBar, parent, "action-bar", kBarId, "control", kBarInnerRml);
        if (bar == nullptr) {
            return;
        }
    }
    if (bar != s_game.bar) {
        release_touch();
        s_game.bar = bar;
        s_barShown = false;
        if (!fetch_children(s_game)) {
            s_game.bar = nullptr;
            return;
        }
    }
    apply_button_visibility(s_game, qa);
    apply_layout(s_game, s_savedProps);
    set_bar_shown(true);
}

void* event_hit(void* event) {
    if (!s_barShown || s_game.bar == nullptr || event == nullptr) {
        return nullptr;
    }
    void* element = s_rmlEventTarget(event);
    for (int depth = 0; element != nullptr && depth < 5; ++depth) {
        if (element == s_game.qa || element == s_game.bottles || element == s_game.bar) {
            return element;
        }
        element = s_rmlGetParentNode(element);
    }
    return nullptr;
}

HookAction before_touch_down(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    void* hit = event_hit(event);
    if (hit == nullptr) {
        return HOOK_CONTINUE;
    }
    if (!s_touchActive && hit != s_game.bar) {
        s_touchActive = true;
        s_touchFinger = s_touchEventId(event);
        s_qaHeld = true;
        s_bottlesHeld = hit == s_game.bottles;
        s_pressedButton = hit;
        set_class(hit, "pressed", true);
    }
    return HOOK_SKIP_ORIGINAL;
}

HookAction before_touch_release(ModContext*, void* args, void*, void*) {
    void* event = args != nullptr ? mods::arg<void*>(args, 1) : nullptr;
    if (!s_touchActive || event == nullptr || s_touchEventId(event) != s_touchFinger) {
        return HOOK_CONTINUE;
    }
    release_touch();
    return HOOK_SKIP_ORIGINAL;
}

void after_pad_read(ModContext*, void*, void*, void*) {
    if (!s_qaHeld) {
        return;
    }
    const u32 bit = controls_binding_bit(CTRL_BIND_QUICK_ACCESS);
    if (bit == 0) {
        return;
    }
    if (s_bottlesHeld) {
        quick_access_request_bottle_page();
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
        set_bar_shown(false);
        return;
    }
    void* root = s_rmlGetParentNode(anchor);
    if (root == nullptr) {
        set_bar_shown(false);
        return;
    }
    sync_game_bar(find_by_id(root, "action-bar"));
}

void editor_forget() {
    s_editorRoot = nullptr;
    s_engineFrame = nullptr;
    s_editorFrame = nullptr;
    s_editorFrameApplied = {};
    s_editor = {};
    s_editorSelected = false;
    s_edit = {};
}

void editor_sync_frame() {
    if (s_editorFrame == nullptr) {
        return;
    }
    set_class(s_editorFrame, "visible", s_editorSelected);
    set_class(s_editor.bar, "editor-selected", s_editorSelected);
    if (!s_editorSelected) {
        s_editorFrameApplied = {};
        return;
    }
    const Vec2 doc = doc_size(s_editorRoot);
    if (doc.x <= 0.0f || doc.y <= 0.0f) {
        return;
    }
    const Rect visual = resolve_layout(s_editProps, doc).visual;
    if (s_editorFrameApplied.element == s_editorFrame && rect_near(s_editorFrameApplied.box, visual)) {
        return;
    }
    set_box(s_editorFrame, visual);
    s_editorFrameApplied.element = s_editorFrame;
    s_editorFrameApplied.box = visual;
}

void editor_set_selected(bool selected) {
    if (s_editorSelected == selected) {
        return;
    }
    s_editorSelected = selected;
    editor_sync_frame();
}

void editor_apply() {
    if (s_editor.bar == nullptr) {
        return;
    }
    apply_layout(s_editor, s_editProps);
    editor_sync_frame();
}

void add_listener(void* element, int handle) {
    if (element == nullptr) {
        return;
    }
    for (int kind = 0; kind < EV_COUNT; ++kind) {
        EditorListener& listener = s_listeners[handle][kind];
        listener.handle = handle;
        listener.kind = kind;
        const std::string type = kEventNames[kind];
        s_rmlAddListener(element, &type, &listener, false);
    }
}

void editor_create(void* root) {
    editor_forget();
    void* anchor = find_by_id(root, "action-bar");
    if (anchor == nullptr) {
        return;
    }
    void* bar = create_element(anchor, root, "action-bar", kBarId, "control", kBarInnerRml);
    if (bar == nullptr) {
        return;
    }
    s_editorRoot = root;
    s_editor.bar = bar;
    if (!fetch_children(s_editor)) {
        editor_forget();
        return;
    }
    s_editorFrame = create_element(anchor, root, "selection-frame", kFrameId, nullptr, kHandleRml);
    s_engineFrame = find_by_id(root, "editor-selection-frame");
    s_editProps = s_savedProps;

    add_listener(bar, HANDLE_MOVE);
    for (int i = HANDLE_LEFT; i <= HANDLE_BOTTOM_RIGHT; ++i) {
        char id[32];
        std::snprintf(id, sizeof(id), "twe-quick-bar-h%d", i);
        add_listener(find_by_id(s_editorFrame, id), i);
    }
    const std::string type = kEventNames[EV_START];
    EditorListener& rootListener = s_listeners[HANDLE_ROOT][EV_START];
    rootListener.handle = HANDLE_ROOT;
    rootListener.kind = EV_START;
    s_rmlAddListener(root, &type, &rootListener, false);
    EditorListener& engineListener = s_listeners[HANDLE_ENGINE][EV_START];
    engineListener.handle = HANDLE_ENGINE;
    engineListener.kind = EV_START;
    for (const char* id : kEngineControlIds) {
        void* control = find_by_id(root, id);
        if (control != nullptr && control != bar) {
            s_rmlAddListener(control, &type, &engineListener, false);
        }
    }
}

void editor_sync(void* root) {
    if (!s_editorApiOk || root == nullptr) {
        return;
    }
    if (root != s_editorRoot || find_by_id(root, kBarId) == nullptr) {
        editor_create(root);
    }
    if (s_editor.bar == nullptr) {
        return;
    }
    const bool qa = g_configQuickAccessEnabled;
    set_property(s_editor.bar, "display", qa ? "flex" : "none");
    if (!qa) {
        editor_set_selected(false);
    }
    apply_button_visibility(s_editor, qa);
    editor_apply();
}

Vec2 pointer_dp(void* event) {
    const Vec2 px = s_touchEventPosition(event);
    const float scale = std::max(dp_scale(s_editorRoot), 1.0f);
    return {px.x / scale, px.y / scale};
}

bool is_corner(int handle) {
    return handle == HANDLE_TOP_LEFT || handle == HANDLE_TOP_RIGHT ||
           handle == HANDLE_BOTTOM_LEFT || handle == HANDLE_BOTTOM_RIGHT;
}

Rect clamp_visual(Rect rect, Vec2 doc) {
    const float minW = std::min(kMinBarWidthDp, doc.x);
    const float minH = std::min(kMinBarHeightDp, doc.y);
    rect.w = std::clamp(rect.w, minW, doc.x);
    rect.h = std::clamp(rect.h, minH, doc.y);
    rect.l = std::clamp(rect.l, 0.0f, std::max(0.0f, doc.x - rect.w));
    rect.t = std::clamp(rect.t, 0.0f, std::max(0.0f, doc.y - rect.h));
    return rect;
}

Rect rect_for_edit(Vec2 pointer, Props& props, Vec2 doc) {
    const PointerEdit& e = s_edit;
    Rect rect = e.startVisual;
    switch (e.handle) {
    case HANDLE_MOVE:
        rect.l += pointer.x - e.startPointer.x;
        rect.t += pointer.y - e.startPointer.y;
        return rect;
    case HANDLE_LEFT: {
        const float right = e.startVisual.l + e.startVisual.w;
        rect.l = pointer.x;
        rect.w = right - rect.l;
        return rect;
    }
    case HANDLE_RIGHT:
        rect.w = pointer.x - e.startVisual.l;
        return rect;
    case HANDLE_TOP: {
        const float bottom = e.startVisual.t + e.startVisual.h;
        rect.t = pointer.y;
        rect.h = bottom - rect.t;
        return rect;
    }
    case HANDLE_BOTTOM:
        rect.h = pointer.y - e.startVisual.t;
        return rect;
    default:
        break;
    }

    const bool left = e.handle == HANDLE_TOP_LEFT || e.handle == HANDLE_BOTTOM_LEFT;
    const bool top = e.handle == HANDLE_TOP_LEFT || e.handle == HANDLE_TOP_RIGHT;
    const Vec2 fixed = {left ? e.startVisual.l + e.startVisual.w : e.startVisual.l,
                        top ? e.startVisual.t + e.startVisual.h : e.startVisual.t};
    const float desiredW = left ? fixed.x - pointer.x : pointer.x - fixed.x;
    const float desiredH = top ? fixed.y - pointer.y : pointer.y - fixed.y;
    const float startW = std::max(e.startVisual.w, 1.0f);
    const float startH = std::max(e.startVisual.h, 1.0f);
    const float minRatio = std::max(kMinBarWidthDp / startW, kMinBarHeightDp / startH);
    const float maxW = left ? fixed.x : doc.x - fixed.x;
    const float maxH = top ? fixed.y : doc.y - fixed.y;
    const float maxRatio = std::max(minRatio, std::min(maxW / startW, maxH / startH));
    const float ratio =
        std::clamp(std::max(desiredW / startW, desiredH / startH), minRatio, maxRatio);
    rect.w = e.startVisual.w * ratio;
    rect.h = e.startVisual.h * ratio;
    rect.l = left ? fixed.x - rect.w : fixed.x;
    rect.t = top ? fixed.y - rect.h : fixed.y;
    props.scale = std::max(e.startProps.scale * ratio, kMinScale);
    return rect;
}

void editor_event(int handle, int kind, void* event) {
    if (event == nullptr || s_editorRoot == nullptr || s_editor.bar == nullptr) {
        return;
    }

    if (handle == HANDLE_ROOT || handle == HANDLE_ENGINE) {
        if (kind == EV_START && !s_edit.active &&
            (handle == HANDLE_ENGINE || s_rmlEventTarget(event) == s_editorRoot)) {
            editor_set_selected(false);
        }
        return;
    }

    if (kind == EV_START) {
        if (s_edit.active || (handle != HANDLE_MOVE && !s_editorSelected)) {
            return;
        }
        const Vec2 doc = doc_size(s_editorRoot);
        if (doc.x <= 0.0f || doc.y <= 0.0f) {
            return;
        }
        s_edit = {};
        s_edit.active = true;
        s_edit.handle = handle;
        s_edit.finger = s_touchEventId(event);
        s_edit.startPointer = pointer_dp(event);
        s_edit.startVisual = resolve_layout(s_editProps, doc).visual;
        s_edit.startProps = s_editProps;
        editor_set_selected(true);
        s_rmlStopPropagation(event);
        return;
    }

    if (!s_edit.active || s_touchEventId(event) != s_edit.finger) {
        return;
    }

    if (kind == EV_MOVE) {
        const Vec2 doc = doc_size(s_editorRoot);
        if (doc.x <= 0.0f || doc.y <= 0.0f) {
            s_rmlStopPropagation(event);
            return;
        }
        const Vec2 pointer = pointer_dp(event);
        if (!s_edit.dragging) {
            const float dx = pointer.x - s_edit.startPointer.x;
            const float dy = pointer.y - s_edit.startPointer.y;
            if (dx * dx + dy * dy < kDragThresholdDp * kDragThresholdDp) {
                s_rmlStopPropagation(event);
                return;
            }
            s_edit.dragging = true;
        }
        Props props = s_edit.startProps;
        Rect rect = clamp_visual(rect_for_edit(pointer, props, doc), doc);
        const float scale = std::max(props.scale, kMinScale);
        if (is_corner(s_edit.handle)) {
            props.scale = std::max(rect.w / std::max(s_edit.startProps.w, 1.0f), kMinScale);
        } else if (s_edit.handle == HANDLE_LEFT || s_edit.handle == HANDLE_RIGHT) {
            props.w = rect.w / scale;
        } else if (s_edit.handle == HANDLE_TOP || s_edit.handle == HANDLE_BOTTOM) {
            props.h = rect.h / scale;
        }
        props.w = std::max(props.w, 1.0f);
        props.h = std::max(props.h, 1.0f);
        props.scale = std::max(props.scale, kMinScale);
        s_editProps = encode_props(rect, doc, props, dock_anchor(rect, doc));
        editor_apply();
        s_rmlStopPropagation(event);
        return;
    }

    if (kind == EV_CANCEL && s_edit.dragging) {
        s_editProps = s_edit.startProps;
        editor_apply();
    }
    s_edit = {};
    s_rmlStopPropagation(event);
}

HookAction before_rml_set_class(ModContext*, void* args, void*, void*) {
    if (args == nullptr) {
        return HOOK_CONTINUE;
    }
    void* element = mods::arg<void*>(args, 0);
    const std::string* className = mods::arg<const std::string*>(args, 1);
    if (element == nullptr || className == nullptr) {
        return HOOK_CONTINUE;
    }
    if (s_inDisplaySync && s_syncAnchor == nullptr && *className == "has-icon") {
        s_syncAnchor = element;
    } else if (s_inEditorSync && s_editorAnchor == nullptr && *className == "docked") {
        s_editorAnchor = element;
    } else if (s_editorSelected && !s_forcingClass && mods::arg<bool>(args, 2) &&
               ((element == s_engineFrame && *className == "visible") ||
                (element != s_editor.bar && *className == "editor-selected"))) {
        s_forcingClass = true;
        s_rmlSetClass(element, className, false);
        s_forcingClass = false;
        return HOOK_SKIP_ORIGINAL;
    }
    return HOOK_CONTINUE;
}

HookAction before_editor_sync(ModContext*, void*, void*, void*) {
    s_inEditorSync = true;
    s_editorAnchor = nullptr;
    return HOOK_CONTINUE;
}

void after_editor_sync(ModContext*, void*, void*, void*) {
    s_inEditorSync = false;
    void* anchor = s_editorAnchor;
    s_editorAnchor = nullptr;
    if (anchor != nullptr) {
        editor_sync(s_rmlGetParentNode(anchor));
    }
}

HookAction before_editor_save(ModContext*, void*, void*, void*) {
    if (s_editorRoot != nullptr) {
        s_savedProps = s_editProps;
        s_game.applied.element = nullptr;
        store_saved_props();
    }
    return HOOK_CONTINUE;
}

void after_editor_reset(ModContext*, void*, void*, void*) {
    if (s_editorRoot == nullptr) {
        return;
    }
    s_edit = {};
    s_editProps = kDefaultProps;
    editor_apply();
}

HookAction before_editor_dtor(ModContext*, void*, void*, void*) {
    editor_forget();
    return HOOK_CONTINUE;
}

void register_layout_var() {
    if (svc_config == nullptr || svc_config->register_var == nullptr) {
        return;
    }
    ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
    d.name = "touchQuickBarLayout";
    d.type = CONFIG_VAR_STRING;
    d.default_string = "";
    if (svc_config->register_var(mod_ctx, &d, &s_layoutVar) != MOD_OK) {
        s_layoutVar = 0;
    }
    load_saved_props();
}

}

void quick_access_mobile_init(const HookService* hook_svc) {
    s_hookSvc = hook_svc;
    if (hook_svc == nullptr) {
        return;
    }

    register_layout_var();

    s_apiOk = resolve_symbol(QA_SYM_RML_GET_ELEMENT_BY_ID, s_rmlGetElementById) &&
              resolve_symbol(QA_SYM_RML_GET_PARENT, s_rmlGetParentNode) &&
              resolve_symbol(QA_SYM_RML_OWNER_DOCUMENT, s_rmlGetOwnerDocument) &&
              resolve_symbol(QA_SYM_RML_GET_CONTEXT, s_rmlGetContext) &&
              resolve_symbol(QA_SYM_RML_EVENT_TARGET, s_rmlEventTarget) &&
              resolve_symbol(QA_SYM_RML_SET_ID, s_rmlSetId) &&
              resolve_symbol(QA_SYM_RML_SET_CLASS, s_rmlSetClass) &&
              resolve_symbol(QA_SYM_RML_SET_PROPERTY, s_rmlSetProperty) &&
              resolve_symbol(QA_SYM_RML_SET_INNER_RML, s_rmlSetInnerRML) &&
              resolve_symbol(QA_SYM_RML_SET_PSEUDO_CLASS, s_rmlSetPseudoClass) &&
              resolve_symbol(QA_SYM_RML_IS_PSEUDO_CLASS_SET, s_rmlIsPseudoClassSet) &&
              resolve_symbol(QA_SYM_TOUCH_EVENT_ID, s_touchEventId) &&
              resolve_symbol(QA_SYM_TOUCH_DOC_SIZE, s_touchDocSize) &&
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

    s_editorApiOk =
        s_apiOk && resolve_symbol(QA_SYM_RML_ADD_LISTENER, s_rmlAddListener) &&
        resolve_symbol(QA_SYM_RML_STOP_PROPAGATION, s_rmlStopPropagation) &&
        resolve_symbol(QA_SYM_TOUCH_EVENT_POSITION, s_touchEventPosition) &&
        resolve_symbol(QA_SYM_TOUCH_DP_SCALE, s_touchDpScale) &&
        mods::hook::add_pre<QaEditorDtorHook>(hook_svc, before_editor_dtor) == MOD_OK &&
        mods::hook::add_pre<QaEditorSyncLayoutsHook>(hook_svc, before_editor_sync) == MOD_OK &&
        mods::hook::add_post<QaEditorSyncLayoutsHook>(hook_svc, after_editor_sync) == MOD_OK &&
        mods::hook::add_pre<QaEditorSaveHook>(hook_svc, before_editor_save) == MOD_OK &&
        mods::hook::add_post<QaEditorResetHook>(hook_svc, after_editor_reset) == MOD_OK;

    if (svc_log != nullptr) {
        svc_log->info(mod_ctx, s_apiOk ? "[QuickAccess/mobile] touch bar available"
                                       : "[QuickAccess/mobile] touch bar unavailable (symbols)");
        svc_log->info(mod_ctx, s_editorApiOk ? "[QuickAccess/mobile] layout editor support on"
                                             : "[QuickAccess/mobile] layout editor support off");
    }
}

bool quick_access_mobile_held() {
    return s_qaHeld;
}

void quick_access_mobile_shutdown() {
    release_touch();
    s_game = {};
    s_barShown = false;
    editor_forget();
}

#else

void quick_access_mobile_init(const HookService*) {}
void quick_access_mobile_shutdown() {}
bool quick_access_mobile_held() {
    return false;
}

#endif

#include "boss_rush_leaderboard.hpp"
#include "boss_rush.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush_darklink.hpp"
#include "boss_rush_equipment.hpp"
#include "boss_rush_hardmode.hpp"
#include "boss_rush_texts.hpp"
#include "boss_rush_timer.hpp"
#include "../quick_access/quick_access_internal.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2AudioMgr.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"

#include "mods/hook.hpp"
#include "mods/svc/http.h"
#include "mods/svc/http.hpp"
#include "mods/svc/log.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern const HttpService* svc_http;
extern const LogService* svc_log;
extern const UiService* svc_ui;

DEFINE_HOOK(&dCamera_c::Run, LeaderboardViewCameraRunHook);
DEFINE_HOOK(&mDoCPd_c::read, LeaderboardViewPadReadHook);

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* kBaseUrl = "https://te.fimmel.dev/api/leaderboard";
constexpr size_t kMinNameLength = 3;
constexpr size_t kMaxNameLength = 16;
constexpr size_t kTokenLength = 32;
constexpr size_t kTopRows = 10;
constexpr size_t kMaxQueued = 32;
constexpr int kMaxSubmitAttempts = 5;
constexpr auto kRefreshInterval = std::chrono::seconds(60);
constexpr auto kRetryDelay = std::chrono::seconds(30);
constexpr auto kRateLimitDelay = std::chrono::seconds(120);
constexpr f32 kPi = 3.14159265f;
constexpr f32 kViewEyeRadius = 760.0f;
constexpr f32 kViewEyeHeight = 200.0f;
constexpr f32 kViewCenterHeight = 145.0f;
constexpr f32 kMasterEyeDistance = 600.0f;
constexpr f32 kMasterEyeHeight = 210.0f;
constexpr f32 kMasterCenterHeight = 120.0f;
constexpr int kEnterFrames = 32;
constexpr f32 kBlurStartProgress = 0.75f;
constexpr int kOrbitSwitchFrames = 18;
constexpr int kFreeSwitchFrames = 24;
constexpr int kExitFrames = 24;
constexpr f32 kStickPress = 0.6f;
constexpr f32 kStickRelease = 0.3f;
constexpr int kStickRepeatDelay = 18;
constexpr int kStickRepeatRate = 9;
constexpr f32 kTopListWidth = 210.0f;

enum class RegState { Off, NeedName, InvalidName, Pending, Registering, Registered, NameTaken, Banned, Offline };

struct Row {
    std::string name;
    u32 cs = 0;
};

struct Board {
    std::vector<Row> rows;
    int youRank = 0;
    u32 youCs = 0;
};

struct Submission {
    std::string category;
    std::string label;
    u32 cs = 0;
    int attempts = 0;
};

const ConfigService* s_cfg = nullptr;
ModContext* s_ctx = nullptr;
ConfigVarHandle s_enabledVar = 0;
ConfigVarHandle s_nameVar = 0;
ConfigVarHandle s_tokenVar = 0;

bool s_enabled = false;
std::string s_name;
std::string s_token;
std::string s_registeredName;
std::string s_registeringName;
RegState s_reg = RegState::Off;
bool s_regInFlight = false;
Clock::time_point s_regRetryAt{};
unsigned int s_generation = 0;

std::unordered_map<std::string, Board> s_boards;
bool s_haveBoards = false;
bool s_fetchInFlight = false;
bool s_fetchFailed = false;
bool s_fetchedOnce = false;
bool s_refreshRequested = false;
Clock::time_point s_lastFetch{};

std::deque<Submission> s_queue;
bool s_submitInFlight = false;
Clock::time_point s_submitRetryAt{};
std::string s_lastSubmitText;

UiElementHandle s_statusElem = 0;
std::string s_appliedStatus;
bool s_rateLimited = false;
std::string s_lastLoggedFailure;

enum class ViewState { Closed, Entering, Browsing, Exiting };
enum class EntryKind { Boss, AllPhases, MasterRush };

struct ViewEntry {
    EntryKind kind = EntryKind::Boss;
    size_t tableIdx = 0;
    f32 angle = 0.0f;
    f32 radius = 0.0f;
};

struct Shot {
    bool orbit = false;
    f32 angle = 0.0f;
    f32 radius = 0.0f;
    cXyz eye;
    cXyz center;
};

ViewState s_view = ViewState::Closed;
std::vector<ViewEntry> s_entries;
int s_viewIndex = 0;
int s_shownIndex = 0;
Shot s_shotFrom;
Shot s_shotTo;
Shot s_shotNow;
int s_moveFrame = 0;
int s_moveFrames = 1;
bool s_needCapture = false;
bool s_hidLink = false;
bool s_hidHud = false;
int s_heldDir = 0;
int s_stickRepeat = 0;
bool s_waitNeutral = false;
f32 s_textFade = 0.0f;

std::string normalize_name(const char* raw) {
    std::string out;
    if (raw == nullptr) return out;
    bool pendingSpace = false;
    for (const char* p = raw; *p != '\0'; ++p) {
        const char c = *p;
        if (c == ' ' || c == '\t') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out += ' ';
            pendingSpace = false;
        }
        out += c;
    }
    return out;
}

bool name_char_ok(char c, bool edge) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return true;
    if (c == '_' || c == '.' || c == '-') return true;
    return c == ' ' && !edge;
}

bool name_valid(const std::string& name) {
    if (name.size() < kMinNameLength || name.size() > kMaxNameLength) return false;
    const char first = name.front();
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) {
        return false;
    }
    for (size_t i = 0; i < name.size(); ++i) {
        if (!name_char_ok(name[i], i == 0 || i + 1 == name.size())) return false;
    }
    return true;
}

std::string escape_json(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) >= 0x20) {
            out += c;
        }
    }
    return out;
}

std::string escape_rml(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

bool token_valid(const std::string& token) {
    if (token.size() != kTokenLength) return false;
    for (const char c : token) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

std::string generate_token() {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd(),
                      static_cast<unsigned int>(Clock::now().time_since_epoch().count())};
    std::mt19937_64 rng(seq);
    static const char kHex[] = "0123456789abcdef";
    std::string token;
    token.reserve(kTokenLength);
    while (token.size() < kTokenLength) {
        std::uint64_t v = rng();
        for (int i = 0; i < 16 && token.size() < kTokenLength; ++i) {
            token += kHex[v & 0xF];
            v >>= 4;
        }
    }
    return token;
}

void ensure_token() {
    if (token_valid(s_token)) return;
    if (s_cfg != nullptr && s_tokenVar != 0) {
        char buf[80] = {};
        size_t len = 0;
        if (s_cfg->get_string(s_ctx, s_tokenVar, buf, sizeof(buf), &len) == MOD_OK) {
            s_token = buf;
        }
    }
    if (token_valid(s_token)) return;
    s_token = generate_token();
    if (s_cfg != nullptr && s_tokenVar != 0) {
        s_cfg->set_string(s_ctx, s_tokenVar, s_token.c_str());
    }
}

std::string slug(const char* name) {
    std::string out;
    bool dash = false;
    for (const char* p = name; p != nullptr && *p != '\0'; ++p) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            if (dash && !out.empty()) out += '-';
            dash = false;
            out += c;
        } else {
            dash = true;
        }
    }
    return out;
}

bool hard_mode_counts() {
    return boss_rush_hardmode_active() && boss_rush_hardmode_service_available();
}

bool dark_link_in_rotation() {
    const size_t count = boss_rush_get_active_gallery_count();
    for (size_t s = 0; s < count; ++s) {
        const size_t idx = boss_rush_get_active_gallery_table_index(s);
        if (idx < g_bossGalleryCount && std::strcmp(g_bossGalleryTable[idx].displayName, kDarkLinkGalleryName) == 0) {
            return true;
        }
    }
    return false;
}

void append_modifiers(std::string& key, std::string* tags, bool masterRush) {
    const auto add = [&](const char* suffix, const char* label) {
        key += suffix;
        if (tags != nullptr) {
            if (!tags->empty()) *tags += "  ";
            *tags += label;
        }
    };
    if (hard_mode_counts()) add("+hm", "Hard Mode");
    if (g_configBossRushVanillaGear) add("+vg", "Vanilla Gear");
    if (masterRush && g_configMasterRushDifficulty == 1) add("+3h", "3 Hearts");
    if (masterRush && dark_link_in_rotation()) add("+dl", "Dark Link");
}

bool is_all_phases_entry(size_t tableIdx) {
    return !g_configBossRushSeparateGanon && tableIdx < g_bossGalleryCount &&
           std::strcmp(g_bossGalleryTable[tableIdx].displayName, "Ganondorf") == 0;
}

std::string boss_category(size_t tableIdx, std::string* tags) {
    std::string key = slug(g_bossGalleryTable[tableIdx].displayName);
    append_modifiers(key, tags, false);
    return key;
}

std::string all_phases_category(std::string* tags) {
    std::string key = "ganondorf-all-phases";
    append_modifiers(key, tags, false);
    return key;
}

std::string master_rush_category(std::string* tags) {
    std::string key = "master-rush";
    append_modifiers(key, tags, true);
    return key;
}

void set_reg(RegState state) {
    s_reg = state;
}

void evaluate_name() {
    if (!s_enabled) {
        set_reg(RegState::Off);
        return;
    }
    if (s_name.empty()) {
        set_reg(RegState::NeedName);
        return;
    }
    if (!name_valid(s_name)) {
        set_reg(RegState::InvalidName);
        return;
    }
    if (s_name == s_registeredName && s_reg == RegState::Registered) return;
    set_reg(RegState::Pending);
}

std::string body_text(const mods::http::Response& response) {
    return std::string(reinterpret_cast<const char*>(response.body.data()), response.body.size());
}

bool body_has(const std::string& body, const char* needle) {
    return body.find(needle) != std::string::npos;
}

const char* http_error_name(HttpError error) {
    switch (error) {
    case HTTP_ERROR_NONE: return "none";
    case HTTP_ERROR_INVALID_URL: return "invalid url";
    case HTTP_ERROR_UNSUPPORTED_SCHEME: return "unsupported scheme";
    case HTTP_ERROR_TIMEOUT: return "timeout";
    case HTTP_ERROR_TOO_LARGE: return "response too large";
    case HTTP_ERROR_CANCELED: return "canceled";
    case HTTP_ERROR_IO: return "io error";
    case HTTP_ERROR_NETWORK: return "network error";
    }
    return "unknown error";
}

void log_failure_line(const std::string& line) {
    if (svc_log == nullptr || s_ctx == nullptr || line == s_lastLoggedFailure) return;
    s_lastLoggedFailure = line;
    svc_log->warn(s_ctx, line.c_str());
}

void log_request_failure(const char* path, const mods::http::Response& response) {
    std::string body = body_text(response);
    for (char& c : body) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    if (body.size() > 200) {
        body.resize(200);
        body += "...";
    }
    char reason[160];
    if (response.error == HTTP_ERROR_NONE) {
        std::snprintf(reason, sizeof(reason), "HTTP %d", response.statusCode);
    } else {
        std::snprintf(reason, sizeof(reason), "%s (%d)%s%s", http_error_name(response.error),
                      static_cast<int>(response.error), response.errorMessage.empty() ? "" : ": ",
                      response.errorMessage.c_str());
    }
    char buf[640];
    std::snprintf(buf, sizeof(buf), "[Leaderboard] %s%s failed: %s%s%s", kBaseUrl, path, reason,
                  body.empty() ? "" : ", response: ", body.c_str());
    log_failure_line(buf);
}

void log_request_not_started(const char* path, ModResult result) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[Leaderboard] %s%s could not be started (result %d)", kBaseUrl,
                  path, static_cast<int>(result));
    log_failure_line(buf);
}

void on_register_response(unsigned int gen, mods::http::Response response) {
    if (gen != s_generation) return;
    s_regInFlight = false;
    const std::string body = body_text(response);
    if (response.ok()) {
        s_lastLoggedFailure.clear();
        s_rateLimited = false;
        s_registeredName = s_registeringName;
        if (s_name == s_registeredName) {
            set_reg(RegState::Registered);
            s_refreshRequested = true;
        } else {
            evaluate_name();
        }
        return;
    }
    if (s_name != s_registeringName) {
        evaluate_name();
        return;
    }
    if (response.statusCode == 409) {
        set_reg(RegState::NameTaken);
    } else if (response.statusCode == 403) {
        set_reg(RegState::Banned);
        s_queue.clear();
    } else if (response.statusCode == 400 && body_has(body, "invalid_name")) {
        set_reg(RegState::InvalidName);
    } else {
        log_request_failure("/register", response);
        s_rateLimited = response.statusCode == 429;
        s_regRetryAt = Clock::now() + (s_rateLimited ? kRateLimitDelay : kRetryDelay);
        set_reg(RegState::Offline);
    }
}

void start_register() {
    if (svc_http == nullptr || s_regInFlight) return;
    ensure_token();
    s_regInFlight = true;
    s_registeringName = s_name;
    set_reg(RegState::Registering);

    mods::http::Request request;
    request.method = HTTP_METHOD_POST;
    request.url = std::string(kBaseUrl) + "/register";
    request.headers.push_back({"Content-Type", "application/json"});
    request.body = "{\"token\":\"" + s_token + "\",\"name\":\"" + escape_json(s_name) + "\"}";
    request.connectTimeoutMs = 8000;
    request.totalTimeoutMs = 12000;
    request.maxBodyBytes = 4096;

    const unsigned int gen = s_generation;
    auto pending = mods::http::request(request, [gen](mods::http::Response response) {
        on_register_response(gen, std::move(response));
    });
    if (!pending) {
        log_request_not_started("/register", pending.result());
        s_regInFlight = false;
        s_regRetryAt = Clock::now() + kRetryDelay;
        set_reg(RegState::Offline);
        return;
    }
    pending.detach();
}

int parse_int_field(const std::string& body, const char* key) {
    const std::string needle = std::string("\"") + key + "\":";
    const size_t pos = body.find(needle);
    if (pos == std::string::npos) return 0;
    return std::atoi(body.c_str() + pos + needle.size());
}

void on_submit_response(unsigned int gen, mods::http::Response response) {
    if (gen != s_generation) return;
    s_submitInFlight = false;
    if (s_queue.empty()) return;
    Submission& item = s_queue.front();
    const std::string body = body_text(response);

    if (response.ok()) {
        s_lastLoggedFailure.clear();
        char t[16];
        boss_rush_timer_format(item.cs, t, sizeof(t));
        const int rank = parse_int_field(body, "rank");
        char buf[160];
        if (body_has(body, "\"improved\":true")) {
            std::snprintf(buf, sizeof(buf), "New online best: %s %s (#%d)", item.label.c_str(), t, rank);
        } else {
            std::snprintf(buf, sizeof(buf), "Uploaded: %s %s (your best stays #%d)", item.label.c_str(), t, rank);
        }
        s_lastSubmitText = buf;
        s_queue.pop_front();
        s_refreshRequested = true;
        return;
    }
    if (response.statusCode == 403 && body_has(body, "not_registered")) {
        s_registeredName.clear();
        evaluate_name();
        return;
    }
    if (response.statusCode == 403) {
        set_reg(RegState::Banned);
        s_queue.clear();
        return;
    }
    log_request_failure("/submit", response);
    if (response.statusCode == 400) {
        s_queue.pop_front();
        return;
    }
    if (response.statusCode == 429) {
        s_submitRetryAt = Clock::now() + kRateLimitDelay;
        return;
    }
    if (++item.attempts >= kMaxSubmitAttempts) {
        s_queue.pop_front();
    }
    s_submitRetryAt = Clock::now() + kRetryDelay;
}

void start_submit() {
    if (svc_http == nullptr || s_submitInFlight || s_queue.empty()) return;
    const Submission& item = s_queue.front();
    s_submitInFlight = true;

    char cs[16];
    std::snprintf(cs, sizeof(cs), "%u", item.cs);
    mods::http::Request request;
    request.method = HTTP_METHOD_POST;
    request.url = std::string(kBaseUrl) + "/submit";
    request.headers.push_back({"Content-Type", "application/json"});
    request.body = "{\"token\":\"" + s_token + "\",\"category\":\"" + escape_json(item.category) +
                   "\",\"timeCs\":" + cs + "}";
    request.connectTimeoutMs = 8000;
    request.totalTimeoutMs = 12000;
    request.maxBodyBytes = 4096;

    const unsigned int gen = s_generation;
    auto pending = mods::http::request(request, [gen](mods::http::Response response) {
        on_submit_response(gen, std::move(response));
    });
    if (!pending) {
        log_request_not_started("/submit", pending.result());
        s_submitInFlight = false;
        s_submitRetryAt = Clock::now() + kRetryDelay;
        return;
    }
    pending.detach();
}

void parse_boards(const std::string& body) {
    s_boards.clear();
    Board* current = nullptr;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        const std::string_view line(body.data() + pos, end - pos);
        pos = end + 1;

        std::string_view fields[3];
        size_t count = 0;
        size_t start = 0;
        while (count < 3) {
            const size_t tab = line.find('\t', start);
            if (tab == std::string_view::npos) {
                fields[count++] = line.substr(start);
                break;
            }
            fields[count++] = line.substr(start, tab - start);
            start = tab + 1;
        }
        if (count >= 2 && fields[0] == "c") {
            current = &s_boards[std::string(fields[1])];
        } else if (count >= 3 && fields[0] == "e" && current != nullptr && current->rows.size() < kTopRows) {
            current->rows.push_back({std::string(fields[1]),
                                     static_cast<u32>(std::strtoul(std::string(fields[2]).c_str(), nullptr, 10))});
        } else if (count >= 3 && fields[0] == "y" && current != nullptr) {
            current->youRank = std::atoi(std::string(fields[1]).c_str());
            current->youCs = static_cast<u32>(std::strtoul(std::string(fields[2]).c_str(), nullptr, 10));
        }
    }
}

void on_fetch_response(unsigned int gen, mods::http::Response response) {
    if (gen != s_generation) return;
    s_fetchInFlight = false;
    s_lastFetch = Clock::now();
    if (!response.ok()) {
        log_request_failure("/top", response);
        s_fetchFailed = true;
        return;
    }
    s_lastLoggedFailure.clear();
    s_fetchFailed = false;
    s_haveBoards = true;
    parse_boards(body_text(response));
}

void start_fetch() {
    if (svc_http == nullptr || s_fetchInFlight) return;
    ensure_token();
    s_fetchInFlight = true;
    s_fetchedOnce = true;
    s_refreshRequested = false;

    mods::http::Request request;
    request.url = std::string(kBaseUrl) + "/top?format=tsv";
    request.headers.push_back({"X-TE-Token", s_token});
    request.connectTimeoutMs = 8000;
    request.totalTimeoutMs = 12000;
    request.maxBodyBytes = 256u * 1024u;

    const unsigned int gen = s_generation;
    auto pending = mods::http::request(request, [gen](mods::http::Response response) {
        on_fetch_response(gen, std::move(response));
    });
    if (!pending) {
        log_request_not_started("/top", pending.result());
        s_fetchInFlight = false;
        s_fetchFailed = true;
        s_lastFetch = Clock::now();
        return;
    }
    pending.detach();
}

void enqueue(std::string category, std::string label, u32 cs) {
    if (!s_enabled || svc_http == nullptr || cs == 0 || s_reg == RegState::Banned) return;
    if (s_queue.size() >= kMaxQueued) s_queue.pop_front();
    s_queue.push_back({std::move(category), std::move(label), cs, 0});
}

std::string status_rml() {
    if (svc_http == nullptr) {
        return "<span style=\"color: #8a94a6;\">Networking is unavailable in this Dusklight build.</span>";
    }
    std::string rml;
    switch (s_reg) {
    case RegState::Off:
        return "<span style=\"color: #8a94a6;\">Online leaderboard is off.</span>";
    case RegState::NeedName:
        return "<span style=\"color: #e0b458;\">Enter a player name to join the leaderboard.</span>";
    case RegState::InvalidName:
        return "<span style=\"color: #e05a5a;\">This name is not allowed. Use 3-16 letters, digits, "
               "spaces or _ . -</span>";
    case RegState::Pending:
    case RegState::Registering:
        return "<span style=\"color: #8a94a6;\">Connecting...</span>";
    case RegState::NameTaken:
        return "<span style=\"color: #e05a5a;\">\"" + escape_rml(s_name) +
               "\" is already taken. Choose another name.</span>";
    case RegState::Banned:
        return "<span style=\"color: #e05a5a;\">This player was removed from the leaderboard.</span>";
    case RegState::Offline:
        if (s_rateLimited) {
            return "<span style=\"color: #e0b458;\">Leaderboard server is busy, trying again in a "
                   "few minutes.</span>";
        }
        return "<span style=\"color: #e0b458;\">Leaderboard server not reachable, retrying...</span>";
    case RegState::Registered:
        rml = "<span style=\"color: #7fd08a;\">Playing as <span style=\"font-weight: bold;\">" +
              escape_rml(s_registeredName) + "</span>.</span>";
        if (!g_configBossRushTimer) {
            rml += "<br/><span style=\"color: #e0b458;\">Turn on the Boss Rush timer to upload times.</span>";
        }
        if (!s_lastSubmitText.empty()) {
            rml += "<br/><span style=\"color: #a8bcd4;\">" + escape_rml(s_lastSubmitText) + "</span>";
        }
        return rml;
    }
    return rml;
}


void read_name_var() {
    if (s_cfg == nullptr || s_nameVar == 0) return;
    char buf[64] = {};
    size_t len = 0;
    if (s_cfg->get_string(s_ctx, s_nameVar, buf, sizeof(buf), &len) == MOD_OK) {
        s_name = normalize_name(buf);
    }
}

std::string fit_text(const std::string& text, f32 maxW, f32 charW) {
    if (boss_rush_texts_measure_width(text.c_str(), charW) <= maxW) return text;
    std::string cut = text;
    while (!cut.empty()) {
        cut.pop_back();
        const std::string candidate = cut + "..";
        if (boss_rush_texts_measure_width(candidate.c_str(), charW) <= maxW) return candidate;
    }
    return "";
}

const char* board_message(const Board* board) {
    if (svc_http == nullptr) return "Online leaderboard unavailable";
    switch (s_reg) {
    case RegState::Off:
        return "Online leaderboard is off";
    case RegState::NeedName:
    case RegState::InvalidName:
        return "Set a player name in Mod Settings";
    case RegState::NameTaken:
        return "Name taken - choose another";
    case RegState::Banned:
        return "Leaderboard access removed";
    default:
        break;
    }
    if (!s_haveBoards) {
        return s_fetchFailed ? "Leaderboard offline" : "Loading...";
    }
    if (board == nullptr || board->rows.empty()) return "No times yet - be the first!";
    return nullptr;
}

Shot orbit_shot(f32 angle, f32 radius) {
    Shot shot;
    shot.orbit = true;
    shot.angle = angle;
    shot.radius = radius;
    const f32 dx = std::sin(angle);
    const f32 dz = std::cos(angle);
    shot.center.set(dx * radius, kBossChamberFloorY + kViewCenterHeight, dz * radius);
    shot.eye.set(dx * kViewEyeRadius, kBossChamberFloorY + kViewEyeHeight, dz * kViewEyeRadius);
    return shot;
}

Shot free_shot(const cXyz& eye, const cXyz& center) {
    Shot shot;
    shot.eye = eye;
    shot.center = center;
    return shot;
}

Shot shot_for(const ViewEntry& entry) {
    if (entry.kind == EntryKind::MasterRush) {
        return free_shot(cXyz(0.0f, kBossChamberFloorY + kMasterEyeHeight, kMasterEyeDistance),
                         cXyz(0.0f, kBossChamberFloorY + kMasterCenterHeight, 0.0f));
    }
    return orbit_shot(entry.angle, entry.radius);
}

Shot link_shot() {
    const daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return s_shotNow;
    const s16 angle = link->shape_angle.y;
    const f32 fx = cM_ssin(angle);
    const f32 fz = cM_scos(angle);
    const cXyz& p = link->current.pos;
    return free_shot(cXyz(p.x - fx * 420.0f, p.y + 140.0f, p.z - fz * 420.0f),
                     cXyz(p.x + fx * 200.0f, p.y + 100.0f, p.z + fz * 200.0f));
}

f32 wrap_pi(f32 a) {
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

Shot blend_shots(const Shot& a, const Shot& b, f32 k) {
    if (a.orbit && b.orbit) {
        return orbit_shot(a.angle + wrap_pi(b.angle - a.angle) * k, a.radius + (b.radius - a.radius) * k);
    }
    return free_shot(a.eye + (b.eye - a.eye) * k, a.center + (b.center - a.center) * k);
}

void begin_move(const Shot& to, int frames) {
    s_shotFrom = s_shotNow;
    s_shotTo = to;
    s_moveFrame = 0;
    s_moveFrames = frames;
}

bool view_moving() {
    return s_needCapture || s_moveFrame < s_moveFrames;
}

void build_entries() {
    s_entries.clear();
    const size_t count = boss_rush_get_active_gallery_count();
    for (size_t slot = 0; slot < count; ++slot) {
        const size_t tableIdx = boss_rush_get_active_gallery_table_index(slot);
        if (tableIdx >= g_bossGalleryCount) continue;
        ViewEntry entry;
        entry.kind = is_all_phases_entry(tableIdx) ? EntryKind::AllPhases : EntryKind::Boss;
        entry.tableIdx = tableIdx;
        entry.angle = (180.0f - static_cast<f32>(slot) * 360.0f / static_cast<f32>(count)) * (kPi / 180.0f);
        entry.radius = kChamberCircleRadius + g_bossGalleryTable[tableIdx].radialOffset;
        s_entries.push_back(entry);
    }
    ViewEntry master;
    master.kind = EntryKind::MasterRush;
    s_entries.push_back(master);
}

void hide_scene_extras() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != nullptr && !link->checkPlayerNoDraw()) {
        link->onPlayerNoDraw();
        s_hidLink = true;
    }
    if (dComIfGp_2dShowCheck()) {
        dComIfGp_2dShowOff();
        s_hidHud = true;
    }
}

void restore_scene_extras() {
    if (s_hidLink) {
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link != nullptr) link->offPlayerNoDraw();
        s_hidLink = false;
    }
    if (s_hidHud) {
        dComIfGp_2dShowOn();
        s_hidHud = false;
    }
}

void close_view_now() {
    restore_scene_extras();
    s_view = ViewState::Closed;
    s_entries.clear();
    s_needCapture = false;
    s_textFade = 0.0f;
}

void begin_exit() {
    if (s_view == ViewState::Closed || s_view == ViewState::Exiting) return;
    restore_scene_extras();
    s_view = ViewState::Exiting;
    begin_move(link_shot(), kExitFrames);
}

void navigate(int dir) {
    if (s_entries.empty()) return;
    const int count = static_cast<int>(s_entries.size());
    s_viewIndex = (s_viewIndex + dir + count) % count;
    const ViewEntry& entry = s_entries[static_cast<size_t>(s_viewIndex)];
    const bool orbitMove = s_shotNow.orbit && entry.kind != EntryKind::MasterRush;
    begin_move(shot_for(entry), orbitMove ? kOrbitSwitchFrames : kFreeSwitchFrames);
    s_view = ViewState::Browsing;
    Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_ITEM, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void place_camera(camera_process_class* cam, const Shot& shot) {
    cam->mCamera.Reset(shot.center, shot.eye);
    cam->mCamera.Start();
    cam->mCamera.SetTrimSize(0);
    fopCamM_SetAngleY(cam, cM_atan2s(shot.center.x - shot.eye.x, shot.center.z - shot.eye.z));
}

void on_view_camera_run_post(ModContext*, void*, void*, void*) {
    if (s_view == ViewState::Closed) return;
    camera_process_class* cam = dComIfGp_getCamera(g_dComIfG_gameInfo.play.getPlayerCameraID(0));
    if (cam == nullptr) return;

    if (s_needCapture) {
        s_shotNow = free_shot(cam->mCamera.Eye(), cam->mCamera.Center());
        s_shotFrom = s_shotNow;
        s_needCapture = false;
    }
    if (s_moveFrame < s_moveFrames) {
        ++s_moveFrame;
        const f32 t = static_cast<f32>(s_moveFrame) / static_cast<f32>(s_moveFrames);
        s_shotNow = blend_shots(s_shotFrom, s_shotTo, t * t * (3.0f - 2.0f * t));
        if (s_moveFrame >= s_moveFrames) {
            s_shotNow = s_shotTo;
            if (s_view == ViewState::Entering) s_view = ViewState::Browsing;
        }
    }
    place_camera(cam, s_shotNow);

    if (s_view == ViewState::Exiting && s_moveFrame >= s_moveFrames) {
        s_view = ViewState::Closed;
        s_entries.clear();
    }
}

void blank_pad(interface_of_controller_pad& pad) {
    pad.mMainStickPosX = 0.0f;
    pad.mMainStickPosY = 0.0f;
    pad.mMainStickValue = 0.0f;
    pad.mMainStickAngle = 0;
    pad.mCStickPosX = 0.0f;
    pad.mCStickPosY = 0.0f;
    pad.mCStickValue = 0.0f;
    pad.mCStickAngle = 0;
    pad.mAnalogA = 0.0f;
    pad.mAnalogB = 0.0f;
    pad.mTriggerLeft = 0.0f;
    pad.mTriggerRight = 0.0f;
    pad.mButtonFlags = 0;
    pad.mPressedButtonFlags = 0;
}

void on_view_pad_read_post(ModContext*, void*, void*, void*) {
    if (s_view == ViewState::Closed) return;
    interface_of_controller_pad& pad = mDoCPd_c::getCpadInfo(PAD_1);
    const f32 stickX = pad.mMainStickPosX;
    const u32 trig = pad.mPressedButtonFlags;
    const u32 hold = pad.mButtonFlags;
    blank_pad(pad);
    if (s_view == ViewState::Exiting || s_needCapture) return;

    if ((trig & (PAD_BUTTON_A | PAD_BUTTON_B)) != 0) {
        Z2GetAudioMgr()->seStart(Z2SE_SY_CURSOR_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        begin_exit();
        return;
    }

    int heldDir = 0;
    if (((hold | trig) & PAD_BUTTON_RIGHT) != 0) {
        heldDir = 1;
    } else if (((hold | trig) & PAD_BUTTON_LEFT) != 0) {
        heldDir = -1;
    } else if (stickX >= kStickPress) {
        heldDir = 1;
    } else if (stickX <= -kStickPress) {
        heldDir = -1;
    } else if (s_heldDir != 0 && std::fabs(stickX) >= kStickRelease && (stickX > 0.0f) == (s_heldDir > 0)) {
        heldDir = s_heldDir;
    }

    if (s_waitNeutral) {
        if (heldDir == 0) s_waitNeutral = false;
        return;
    }

    int dir = 0;
    if (heldDir == 0) {
        s_heldDir = 0;
    } else if (heldDir != s_heldDir) {
        s_heldDir = heldDir;
        s_stickRepeat = kStickRepeatDelay;
        dir = heldDir;
    } else if (--s_stickRepeat <= 0) {
        s_stickRepeat = kStickRepeatRate;
        dir = heldDir;
    }
    if (dir != 0) navigate(dir);
}

void draw_centered(const char* text, f32 cx, f32 y, f32 charW, f32 charH,
                   JUtility::TColor top, JUtility::TColor bottom, u8 alpha) {
    const f32 w = boss_rush_texts_measure_width(text, charW);
    boss_rush_texts_draw_label(text, cx - w * 0.5f, y, charW, charH, top, bottom, alpha);
}

void draw_close_hint(f32 cx, f32 y, u8 alpha) {
    constexpr f32 kHintW = 12.0f, kHintH = 14.0f;
    constexpr f32 kIconH = 20.0f;
    constexpr f32 kIconGap = 6.0f;
    const JUtility::TColor top(255, 248, 210, 255);
    const JUtility::TColor bottom(235, 185, 65, 255);
    if (!qa_hint_button_ready()) {
        draw_centered("A  Close", cx, y, kHintW, kHintH, top, bottom, alpha);
        return;
    }
    const f32 iconW = qa_hint_button_width(0, kIconH);
    const f32 textW = boss_rush_texts_measure_width("Close", kHintW);
    const f32 x = cx - (iconW + kIconGap + textW) * 0.5f;
    qa_draw_hint_button(0, x, y - kIconH * 0.68f, kIconH, alpha);
    boss_rush_texts_draw_label("Close", x + iconW + kIconGap, y, kHintW, kHintH, top, bottom, alpha);
}

void draw_top_list(const Board* board, f32 x0, f32 x1, f32 y, u8 alpha) {
    constexpr f32 kHeadW = 14.0f, kHeadH = 17.0f;
    constexpr f32 kRowW = 12.0f, kRowH = 14.0f;
    constexpr f32 kRowStep = 18.0f;

    boss_rush_texts_draw_label("Online Top 10", x0, y, kHeadW, kHeadH,
                               JUtility::TColor(215, 222, 235, 255), JUtility::TColor(150, 165, 185, 255), alpha);
    y += kHeadH + 10.0f;

    const char* message = board_message(board);
    if (message != nullptr) {
        boss_rush_texts_draw_label(message, x0, y, kRowW, kRowH,
                                   JUtility::TColor(220, 220, 220, 255), JUtility::TColor(170, 170, 170, 255), alpha);
        if (s_reg == RegState::Off) {
            boss_rush_texts_draw_label("Turn it on in Mod Settings", x0, y + kRowStep, kRowW, kRowH,
                                       JUtility::TColor(190, 190, 190, 255), JUtility::TColor(140, 140, 140, 255), alpha);
        }
        return;
    }

    const f32 nameX = x0 + 28.0f;
    for (size_t i = 0; i < board->rows.size(); ++i) {
        const Row& row = board->rows[i];
        const bool you = static_cast<int>(i) + 1 == board->youRank;
        JUtility::TColor top(230, 230, 230, 255);
        JUtility::TColor bottom(185, 185, 185, 255);
        if (you) {
            top = JUtility::TColor(170, 255, 180, 255);
            bottom = JUtility::TColor(90, 210, 120, 255);
        } else if (i == 0) {
            top = JUtility::TColor(255, 230, 130, 255);
            bottom = JUtility::TColor(235, 175, 50, 255);
        } else if (i == 1) {
            top = JUtility::TColor(235, 240, 250, 255);
            bottom = JUtility::TColor(170, 180, 195, 255);
        } else if (i == 2) {
            top = JUtility::TColor(240, 190, 140, 255);
            bottom = JUtility::TColor(190, 120, 70, 255);
        }

        char rank[8];
        std::snprintf(rank, sizeof(rank), "%zu.", i + 1);
        char t[16];
        boss_rush_timer_format(row.cs, t, sizeof(t));
        const f32 tW = boss_rush_texts_measure_width(t, kRowW);
        const std::string name = fit_text(row.name, x1 - tW - 10.0f - nameX, kRowW);

        boss_rush_texts_draw_label(rank, x0, y, kRowW, kRowH, top, bottom, alpha);
        boss_rush_texts_draw_label(name.c_str(), nameX, y, kRowW, kRowH, top, bottom, alpha);
        boss_rush_texts_draw_label(t, x1 - tW, y, kRowW, kRowH, top, bottom, alpha);
        y += kRowStep;
    }

    if (board->youRank > static_cast<int>(kTopRows)) {
        y += 6.0f;
        char line[24];
        std::snprintf(line, sizeof(line), "%d.", board->youRank);
        char t[16];
        boss_rush_timer_format(board->youCs, t, sizeof(t));
        const f32 tW = boss_rush_texts_measure_width(t, kRowW);
        const JUtility::TColor top(170, 255, 180, 255);
        const JUtility::TColor bottom(90, 210, 120, 255);
        boss_rush_texts_draw_label(line, x0, y, kRowW, kRowH, top, bottom, alpha);
        boss_rush_texts_draw_label("You", nameX, y, kRowW, kRowH, top, bottom, alpha);
        boss_rush_texts_draw_label(t, x1 - tW, y, kRowW, kRowH, top, bottom, alpha);
    }
}

}

void boss_rush_leaderboard_init(const ConfigService* config_svc, ModContext* mod_ctx,
                                ConfigVarHandle enabled_var, ConfigVarHandle name_var,
                                ConfigVarHandle token_var) {
    s_cfg = config_svc;
    s_ctx = mod_ctx;
    s_enabledVar = enabled_var;
    s_nameVar = name_var;
    s_tokenVar = token_var;

    bool enabled = false;
    if (s_cfg != nullptr && s_enabledVar != 0) {
        s_cfg->get_bool(s_ctx, s_enabledVar, &enabled);
    }
    read_name_var();
    boss_rush_leaderboard_set_enabled(enabled);
}

void boss_rush_leaderboard_shutdown() {
    ++s_generation;
    s_regInFlight = false;
    s_fetchInFlight = false;
    s_submitInFlight = false;
    s_queue.clear();
    s_boards.clear();
    s_haveBoards = false;
    s_statusElem = 0;
    close_view_now();
}

void boss_rush_leaderboard_set_enabled(bool enabled) {
    if (enabled == s_enabled && s_reg != RegState::Off) return;
    s_enabled = enabled;
    if (!enabled) {
        ++s_generation;
        s_regInFlight = false;
        s_fetchInFlight = false;
        s_submitInFlight = false;
        s_queue.clear();
        s_registeredName.clear();
        s_lastSubmitText.clear();
        s_boards.clear();
        s_haveBoards = false;
        s_fetchedOnce = false;
    } else {
        ensure_token();
        s_refreshRequested = true;
    }
    evaluate_name();
}

void boss_rush_leaderboard_set_name(const char* name) {
    const std::string normalized = normalize_name(name);
    if (normalized == s_name && s_reg != RegState::Offline) return;
    s_name = normalized;
    evaluate_name();
}

bool boss_rush_leaderboard_enabled() {
    return s_enabled;
}

void boss_rush_leaderboard_update() {
    if (s_view != ViewState::Closed &&
        (daAlink_getAlinkActorClass() == nullptr || !is_in_boss_rush_chamber() ||
         is_boss_rush_transition_in_flight() || dMeter2Info_getWindowStatus() != 0)) {
        close_view_now();
    }

    if (!s_enabled || svc_http == nullptr) return;
    const Clock::time_point now = Clock::now();

    if (!s_regInFlight &&
        (s_reg == RegState::Pending || (s_reg == RegState::Offline && now >= s_regRetryAt))) {
        start_register();
    }

    if (s_reg == RegState::Registered && !s_submitInFlight && !s_queue.empty() && now >= s_submitRetryAt) {
        start_submit();
    }

    if (!s_fetchInFlight && !s_submitInFlight && is_in_boss_rush_chamber()) {
        const bool stale = !s_fetchedOnce || now - s_lastFetch >= kRefreshInterval;
        if (s_refreshRequested || stale) {
            start_fetch();
        }
    }
}

void boss_rush_leaderboard_submit_boss(int tableIndex, unsigned int cs) {
    if (tableIndex < 0 || static_cast<size_t>(tableIndex) >= g_bossGalleryCount) return;
    enqueue(boss_category(static_cast<size_t>(tableIndex), nullptr),
            g_bossGalleryTable[tableIndex].displayName, cs);
}

void boss_rush_leaderboard_submit_all_phases(unsigned int cs) {
    enqueue(all_phases_category(nullptr), "Ganondorf (All Phases)", cs);
}

void boss_rush_leaderboard_submit_master_rush(unsigned int cs) {
    enqueue(master_rush_category(nullptr), "Master Rush", cs);
}

void boss_rush_leaderboard_add_status(UiElementHandle pane) {
    if (svc_ui == nullptr || s_ctx == nullptr) return;
    s_statusElem = 0;
    s_appliedStatus = status_rml();
    svc_ui->pane_add_rml(s_ctx, pane, s_appliedStatus.c_str(), &s_statusElem);
}

void boss_rush_leaderboard_status_update() {
    if (svc_ui == nullptr || s_ctx == nullptr || s_statusElem == 0) return;
    std::string rml = status_rml();
    if (rml == s_appliedStatus) return;
    s_appliedStatus = std::move(rml);
    if (svc_ui->elem_set_rml(s_ctx, s_statusElem, s_appliedStatus.c_str()) != MOD_OK) {
        s_statusElem = 0;
    }
}

void boss_rush_leaderboard_install_hooks(const HookService* hook_svc) {
    if (hook_svc == nullptr) return;
    mods::hook::add_post<LeaderboardViewCameraRunHook>(hook_svc, on_view_camera_run_post);
    mods::hook::add_post<LeaderboardViewPadReadHook>(hook_svc, on_view_pad_read_post);
}

void boss_rush_leaderboard_view_open() {
    if (s_view != ViewState::Closed) return;
    if (daAlink_getAlinkActorClass() == nullptr || !is_in_boss_rush_chamber()) return;
    build_entries();
    if (s_entries.empty()) return;

    s_viewIndex = 0;
    s_shownIndex = 0;
    s_view = ViewState::Entering;
    s_needCapture = true;
    s_shotTo = shot_for(s_entries.front());
    s_moveFrame = 0;
    s_moveFrames = kEnterFrames;
    s_heldDir = 0;
    s_stickRepeat = kStickRepeatDelay;
    s_waitNeutral = true;
    s_textFade = 0.0f;
    hide_scene_extras();
    if (s_enabled) s_refreshRequested = true;
}

bool boss_rush_leaderboard_view_active() {
    return s_view != ViewState::Closed;
}

bool boss_rush_leaderboard_view_focused() {
    if (s_view == ViewState::Browsing) return true;
    if (s_view != ViewState::Entering || s_needCapture) return false;
    return static_cast<f32>(s_moveFrame) >= static_cast<f32>(s_moveFrames) * kBlurStartProgress;
}

void draw_boss_rush_leaderboard_view() {
    if (s_view == ViewState::Closed || s_entries.empty()) {
        s_textFade = 0.0f;
        return;
    }
    if (s_shownIndex != s_viewIndex && s_textFade < 0.01f) {
        s_shownIndex = s_viewIndex;
    }
    const bool show = s_view != ViewState::Exiting && !view_moving() && s_shownIndex == s_viewIndex;
    s_textFade += ((show ? 1.0f : 0.0f) - s_textFade) * (show ? 0.12f : 0.35f);
    if (s_textFade < 0.01f) {
        s_textFade = 0.0f;
        return;
    }

    const ViewEntry& entry = s_entries[static_cast<size_t>(s_shownIndex)];
    std::string tags;
    std::string category;
    std::string title;
    const char* location = nullptr;
    u32 bestCs = 0;
    bool hasBest = false;
    switch (entry.kind) {
    case EntryKind::MasterRush:
        category = master_rush_category(&tags);
        title = "Master Rush";
        location = "Every boss in a row";
        hasBest = boss_rush_timer_chain_best_cs(&bestCs);
        break;
    case EntryKind::AllPhases:
        category = all_phases_category(&tags);
        title = "Ganondorf (All Phases)";
        location = g_bossGalleryTable[entry.tableIdx].location;
        hasBest = boss_rush_timer_all_phases_best_cs(&bestCs);
        break;
    case EntryKind::Boss:
        category = boss_category(entry.tableIdx, &tags);
        title = g_bossGalleryTable[entry.tableIdx].displayName;
        location = g_bossGalleryTable[entry.tableIdx].location;
        hasBest = boss_rush_timer_best_cs(static_cast<int>(entry.tableIdx), &bestCs);
        break;
    }
    const bool hard = hard_mode_counts();
    const auto boardIt = s_enabled ? s_boards.find(category) : s_boards.end();
    const Board* board = boardIt != s_boards.end() ? &boardIt->second : nullptr;

    f32 minX = mDoGph_gInf_c::getSafeMinXF();
    f32 maxX = mDoGph_gInf_c::getSafeMaxXF();
    if (!(maxX > minX + 200.0f)) {
        minX = 0.0f;
        maxX = 640.0f;
    }
    minX += 24.0f;
    maxX -= 24.0f;
    f32 topY = mDoGph_gInf_c::getMinYF();
    if (topY < 0.0f || topY > 200.0f) topY = 0.0f;
    f32 bottomY = mDoGph_gInf_c::getSafeMaxYF();
    if (bottomY <= topY + 200.0f) bottomY = 448.0f;
    const f32 cx = (minX + maxX) * 0.5f;
    const u8 a = static_cast<u8>(255.0f * s_textFade);

    J2DFillBox(0.0f, 0.0f, 0.0f, 0.0f, JUtility::TColor(0, 0, 0, 0));

    f32 y = topY + 30.0f;
    draw_centered(title.c_str(), cx, y, 24.0f, 28.0f,
                  hard ? JUtility::TColor(255, 170, 150, 255) : JUtility::TColor(255, 236, 170, 255),
                  hard ? JUtility::TColor(220, 40, 30, 255) : JUtility::TColor(255, 190, 60, 255), a);
    y += 31.0f;
    if (location != nullptr && location[0] != '\0') {
        draw_centered(location, cx, y, 14.0f, 17.0f,
                      JUtility::TColor(230, 230, 230, 255), JUtility::TColor(180, 180, 180, 255), a);
        y += 20.0f;
    }
    if (!tags.empty()) {
        draw_centered(tags.c_str(), cx, y, 12.0f, 14.0f,
                      JUtility::TColor(255, 170, 150, 255), JUtility::TColor(220, 90, 70, 255), a);
    }

    const f32 leftX = minX + (cx - minX) * 0.36f;
    const f32 colY = topY + 165.0f;
    draw_centered("Your Best", leftX, colY, 14.0f, 17.0f,
                  JUtility::TColor(215, 222, 235, 255), JUtility::TColor(150, 165, 185, 255), a);
    char best[16];
    if (hasBest) {
        boss_rush_timer_format(bestCs, best, sizeof(best));
    } else {
        std::snprintf(best, sizeof(best), "--:--.--");
    }
    draw_centered(best, leftX, colY + 24.0f, 26.0f, 30.0f,
                  hasBest ? JUtility::TColor(255, 236, 170, 255) : JUtility::TColor(200, 200, 200, 255),
                  hasBest ? JUtility::TColor(255, 190, 60, 255) : JUtility::TColor(140, 140, 140, 255), a);
    if (board != nullptr && board->youRank > 0) {
        char rank[32];
        std::snprintf(rank, sizeof(rank), "Online Rank %d", board->youRank);
        draw_centered(rank, leftX, colY + 64.0f, 13.0f, 16.0f,
                      JUtility::TColor(170, 255, 180, 255), JUtility::TColor(90, 210, 120, 255), a);
    }

    const f32 rightX0 = cx + (maxX - cx) * 0.24f;
    const f32 rightX1 = rightX0 + kTopListWidth < maxX ? rightX0 + kTopListWidth : maxX;
    draw_top_list(board, rightX0, rightX1, topY + 140.0f, a);

    char page[32];
    std::snprintf(page, sizeof(page), "<   %d / %d   >", s_shownIndex + 1, static_cast<int>(s_entries.size()));
    draw_centered(page, cx, bottomY - 52.0f, 14.0f, 17.0f,
                  JUtility::TColor(230, 230, 230, 255), JUtility::TColor(180, 180, 180, 255), a);
    draw_close_hint(cx, bottomY - 28.0f, a);

    J2DGrafContext* port = dComIfGp_getCurrentGrafPort();
    if (port) port->setup2D();
}

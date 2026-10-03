#include "settings_transfer.hpp"

#include "mods/svc/file.h"
#include "mods/svc/host.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern const HostService* svc_host;
extern const FileService* svc_file;
extern const UiService* svc_ui;

namespace settings_transfer {
namespace {

constexpr size_t kMaxImportBytes = 4u * 1024u * 1024u;

constexpr const char* kExcludedNames[] = {
    "bossRushBestTimes",
    "bossRushChainBest",
    "bossRushAllPhasesBest",
    "bossRushLeaderboardToken",
};

struct TrackedVar {
    std::string name;
    ConfigVarType type;
    ConfigVarHandle handle;
};

const ConfigService* g_base = nullptr;
ConfigService g_tracking{};
std::vector<TrackedVar> g_vars;
UiElementHandle g_button = 0;
std::filesystem::path g_exportTemp;

bool is_excluded(std::string_view name) {
    for (const char* excluded : kExcludedNames) {
        if (name == excluded) return true;
    }
    return false;
}

ModResult tracking_register_var(
    ModContext* ctx, const ConfigVarDesc* desc, ConfigVarHandle* out_handle) {
    ConfigVarHandle handle = 0;
    const ModResult result = g_base->register_var(ctx, desc, &handle);
    if (out_handle != nullptr) *out_handle = handle;
    if (result == MOD_OK && handle != 0 && desc != nullptr && desc->name != nullptr) {
        g_vars.push_back({desc->name, desc->type, handle});
    }
    return result;
}

ModResult tracking_unregister_var(ModContext* ctx, ConfigVarHandle var) {
    g_vars.erase(std::remove_if(g_vars.begin(), g_vars.end(),
                     [var](const TrackedVar& tracked) { return tracked.handle == var; }),
        g_vars.end());
    return g_base->unregister_var(ctx, var);
}

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

bool read_value(const TrackedVar& var, std::string* out) {
    if (g_base == nullptr) return false;
    switch (var.type) {
        case CONFIG_VAR_BOOL: {
            bool value = false;
            if (g_base->get_bool(mod_ctx, var.handle, &value) != MOD_OK) return false;
            *out = value ? "true" : "false";
            return true;
        }
        case CONFIG_VAR_INT: {
            int64_t value = 0;
            if (g_base->get_int(mod_ctx, var.handle, &value) != MOD_OK) return false;
            *out = std::to_string(value);
            return true;
        }
        case CONFIG_VAR_FLOAT: {
            double value = 0.0;
            if (g_base->get_float(mod_ctx, var.handle, &value) != MOD_OK) return false;
            if (!std::isfinite(value)) return false;
            char buffer[40];
            std::snprintf(buffer, sizeof(buffer), "%.17g", value);
            *out = buffer;
            return true;
        }
        case CONFIG_VAR_STRING: {
            size_t length = 0;
            if (g_base->get_string(mod_ctx, var.handle, nullptr, 0, &length) != MOD_OK) {
                return false;
            }
            std::string value(length + 1, '\0');
            if (g_base->get_string(mod_ctx, var.handle, value.data(), value.size(), &length) !=
                MOD_OK) {
                return false;
            }
            value.resize(length);
            *out = "\"" + json_escape(value) + "\"";
            return true;
        }
    }
    return false;
}

struct JsonReader {
    std::string_view text;
    size_t pos = 0;

    void skip_ws() {
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r')) {
            ++pos;
        }
    }

    bool peek(char* out) {
        skip_ws();
        if (pos >= text.size()) return false;
        *out = text[pos];
        return true;
    }

    bool consume(char c) {
        skip_ws();
        if (pos < text.size() && text[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool read_literal(std::string_view word) {
        skip_ws();
        if (text.substr(pos, word.size()) != word) return false;
        pos += word.size();
        return true;
    }

    bool read_hex4(uint32_t* out) {
        if (pos + 4 > text.size()) return false;
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[pos++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
            else return false;
        }
        *out = value;
        return true;
    }

    static void append_utf8(std::string* out, uint32_t cp) {
        if (cp < 0x80) {
            *out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            *out += static_cast<char>(0xC0 | (cp >> 6));
            *out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            *out += static_cast<char>(0xE0 | (cp >> 12));
            *out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            *out += static_cast<char>(0xF0 | (cp >> 18));
            *out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            *out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool read_string(std::string* out) {
        skip_ws();
        if (pos >= text.size() || text[pos] != '"') return false;
        ++pos;
        out->clear();
        while (pos < text.size()) {
            const char c = text[pos++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') {
                *out += c;
                continue;
            }
            if (pos >= text.size()) return false;
            const char escape = text[pos++];
            switch (escape) {
                case '"':
                case '\\':
                case '/': *out += escape; break;
                case 'b': *out += '\b'; break;
                case 'f': *out += '\f'; break;
                case 'n': *out += '\n'; break;
                case 'r': *out += '\r'; break;
                case 't': *out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (!read_hex4(&cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        uint32_t low = 0;
                        if (pos + 2 > text.size() || text[pos] != '\\' || text[pos + 1] != 'u') {
                            return false;
                        }
                        pos += 2;
                        if (!read_hex4(&low) || low < 0xDC00 || low > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return false;
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool read_number(std::string* out) {
        skip_ws();
        const size_t start = pos;
        if (pos < text.size() && text[pos] == '-') ++pos;
        while (pos < text.size() &&
               (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.' ||
                   text[pos] == 'e' || text[pos] == 'E' || text[pos] == '+' || text[pos] == '-')) {
            ++pos;
        }
        if (pos == start) return false;
        out->assign(text.substr(start, pos - start));
        return true;
    }

    bool skip_value(int depth = 0) {
        if (depth > 64) return false;
        char c = 0;
        if (!peek(&c)) return false;
        if (c == '"') {
            std::string ignored;
            return read_string(&ignored);
        }
        if (c == '{') {
            ++pos;
            if (consume('}')) return true;
            do {
                std::string key;
                if (!read_string(&key) || !consume(':') || !skip_value(depth + 1)) return false;
            } while (consume(','));
            return consume('}');
        }
        if (c == '[') {
            ++pos;
            if (consume(']')) return true;
            do {
                if (!skip_value(depth + 1)) return false;
            } while (consume(','));
            return consume(']');
        }
        if (read_literal("true") || read_literal("false") || read_literal("null")) return true;
        std::string ignored;
        return read_number(&ignored);
    }
};

struct ImportedValue {
    enum Kind { Bool, Number, String, Other };
    Kind kind = Other;
    bool boolValue = false;
    std::string text;
};

bool read_scalar(JsonReader& reader, ImportedValue* out) {
    char c = 0;
    if (!reader.peek(&c)) return false;
    if (c == '"') {
        out->kind = ImportedValue::String;
        return reader.read_string(&out->text);
    }
    if (reader.read_literal("true")) {
        out->kind = ImportedValue::Bool;
        out->boolValue = true;
        return true;
    }
    if (reader.read_literal("false")) {
        out->kind = ImportedValue::Bool;
        out->boolValue = false;
        return true;
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        out->kind = ImportedValue::Number;
        return reader.read_number(&out->text);
    }
    out->kind = ImportedValue::Other;
    return reader.skip_value();
}

bool parse_settings_file(std::string_view text,
    std::vector<std::pair<std::string, ImportedValue>>* out, std::string* error) {
    JsonReader reader{text};
    if (text.substr(0, 3) == "\xEF\xBB\xBF") reader.pos = 3;

    const char* invalid = "The file is not valid JSON.";
    if (!reader.consume('{')) {
        *error = invalid;
        return false;
    }

    bool sawSettings = false;
    if (!reader.consume('}')) {
        do {
            std::string key;
            if (!reader.read_string(&key) || !reader.consume(':')) {
                *error = invalid;
                return false;
            }
            if (key == "settings") {
                if (!reader.consume('{')) {
                    *error = "\"settings\" must be an object.";
                    return false;
                }
                sawSettings = true;
                if (!reader.consume('}')) {
                    do {
                        std::string name;
                        ImportedValue value;
                        if (!reader.read_string(&name) || !reader.consume(':') ||
                            !read_scalar(reader, &value)) {
                            *error = invalid;
                            return false;
                        }
                        out->emplace_back(std::move(name), std::move(value));
                    } while (reader.consume(','));
                    if (!reader.consume('}')) {
                        *error = invalid;
                        return false;
                    }
                }
            } else if (!reader.skip_value()) {
                *error = invalid;
                return false;
            }
        } while (reader.consume(','));
        if (!reader.consume('}')) {
            *error = invalid;
            return false;
        }
    }

    if (!sawSettings) {
        *error = "This is not a Twilit Essentials settings file.";
        return false;
    }
    return true;
}

bool parse_int(const std::string& text, int64_t* out) {
    if (text.find_first_of(".eE") != std::string::npos) {
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(value)) return false;
        if (value < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            value > static_cast<double>(std::numeric_limits<int64_t>::max())) {
            return false;
        }
        *out = static_cast<int64_t>(std::llround(value));
        return true;
    }
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE) return false;
    *out = static_cast<int64_t>(value);
    return true;
}

bool apply_value(const TrackedVar& var, const ImportedValue& value) {
    switch (var.type) {
        case CONFIG_VAR_BOOL:
            return value.kind == ImportedValue::Bool &&
                   g_base->set_bool(mod_ctx, var.handle, value.boolValue) == MOD_OK;
        case CONFIG_VAR_INT: {
            int64_t parsed = 0;
            return value.kind == ImportedValue::Number && parse_int(value.text, &parsed) &&
                   g_base->set_int(mod_ctx, var.handle, parsed) == MOD_OK;
        }
        case CONFIG_VAR_FLOAT: {
            if (value.kind != ImportedValue::Number) return false;
            char* end = nullptr;
            const double parsed = std::strtod(value.text.c_str(), &end);
            if (end == value.text.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
            return g_base->set_float(mod_ctx, var.handle, parsed) == MOD_OK;
        }
        case CONFIG_VAR_STRING:
            return value.kind == ImportedValue::String &&
                   g_base->set_string(mod_ctx, var.handle, value.text.c_str()) == MOD_OK;
    }
    return false;
}

void push_toast(const char* body_rml) {
    if (svc_ui == nullptr) return;
    UiToastDesc desc = UI_TOAST_DESC_INIT;
    desc.body_rml = body_rml;
    svc_ui->push_toast(mod_ctx, &desc);
}

std::string rml_escape(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            default: out += c;
        }
    }
    return out;
}

void show_error(const char* title, std::string_view detail) {
    if (svc_ui == nullptr) return;

    static UiDialogAction action;
    action.struct_size = sizeof(UiDialogAction);
    action.label = "Close";
    action.on_pressed = nullptr;
    action.user_data = nullptr;
    action.keep_open = false;

    static std::string body;
    body = "<p>" + rml_escape(detail) + "</p>";

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = title;
    desc.body_rml = body.c_str();
    desc.variant = UI_DIALOG_DANGER;
    desc.actions = &action;
    desc.action_count = 1;

    UiDialogHandle handle = 0;
    svc_ui->dialog_push(mod_ctx, &desc, &handle);
}

std::filesystem::path path_from_utf8(const char* text) {
    return std::filesystem::path(reinterpret_cast<const char8_t*>(text));
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

void remove_export_temp() {
    if (g_exportTemp.empty()) return;
    std::error_code ec;
    std::filesystem::remove(g_exportTemp, ec);
    g_exportTemp.clear();
}

bool write_export_temp(std::filesystem::path* out) {
    if (svc_host == nullptr || mod_ctx == nullptr) return false;
    const char* dir = nullptr;
    if (svc_host->data_dir(mod_ctx, &dir) != MOD_OK || dir == nullptr || dir[0] == '\0') {
        return false;
    }
    const std::filesystem::path path = path_from_utf8(dir) / kFileName;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    const std::string json = export_json();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    file.flush();
    if (!file) return false;
    *out = path;
    return true;
}

void on_export_done(ModContext*, ModResult status, const char* const*, uint32_t, const char* error,
    void*) {
    remove_export_temp();
    if (status == MOD_OK) {
        push_toast("Settings exported.");
    } else if (status != MOD_UNAVAILABLE) {
        show_error("Settings export failed",
            error != nullptr && error[0] != '\0' ? error : "The settings file could not be saved.");
    }
}

void on_export_selected(ModContext* ctx, void*) {
    if (svc_file == nullptr) {
        show_error("Settings export failed", "File access is unavailable in this build.");
        return;
    }
    remove_export_temp();
    std::filesystem::path path;
    if (!write_export_temp(&path)) {
        show_error("Settings export failed", "The settings file could not be created.");
        return;
    }
    g_exportTemp = path;
    const std::string location = path_to_utf8(path);
    const ModResult result =
        svc_file->export_file(ctx, location.c_str(), kFileName, on_export_done, nullptr);
    if (result != MOD_OK) {
        remove_export_temp();
        show_error("Settings export failed", result == MOD_CONFLICT ?
                                                 "Another file dialog is already open." :
                                                 "The save dialog could not be opened.");
    }
}

void on_import_picked(ModContext* ctx, ModResult status, const char* const* locations,
    uint32_t location_count, const char* error, void*) {
    if (status == MOD_UNAVAILABLE) return;
    if (status != MOD_OK || locations == nullptr || location_count == 0 ||
        locations[0] == nullptr) {
        show_error("Settings import failed",
            error != nullptr && error[0] != '\0' ? error : "No file was selected.");
        return;
    }
    if (g_base == nullptr) {
        show_error("Settings import failed", "Settings are unavailable in this build.");
        return;
    }

    FileBuffer buffer = FILE_BUFFER_INIT;
    if (svc_file->read_all(ctx, locations[0], &buffer) != MOD_OK) {
        show_error("Settings import failed", "The file could not be read.");
        return;
    }
    std::string text;
    if (buffer.data != nullptr && buffer.size <= kMaxImportBytes) {
        text.assign(static_cast<const char*>(buffer.data), buffer.size);
    }
    const bool tooLarge = buffer.size > kMaxImportBytes;
    svc_file->free(ctx, &buffer);
    if (tooLarge) {
        show_error("Settings import failed", "The file is too large to be a settings file.");
        return;
    }

    std::vector<std::pair<std::string, ImportedValue>> values;
    std::string parseError;
    if (!parse_settings_file(text, &values, &parseError)) {
        show_error("Settings import failed", parseError);
        return;
    }

    int applied = 0;
    int skipped = 0;
    for (const auto& entry : values) {
        const std::string& name = entry.first;
        if (is_excluded(name)) continue;
        const auto var = std::find_if(g_vars.begin(), g_vars.end(),
            [&name](const TrackedVar& tracked) { return tracked.name == name; });
        if (var != g_vars.end() && apply_value(*var, entry.second)) {
            ++applied;
        } else {
            ++skipped;
        }
    }

    std::string message = "Imported " + std::to_string(applied) + " settings.";
    if (skipped > 0) {
        message += " " + std::to_string(skipped) + " unknown or invalid entries were skipped.";
    }
    push_toast(message.c_str());
}

void on_import_selected(ModContext* ctx, void*) {
    if (svc_file == nullptr) {
        show_error("Settings import failed", "File access is unavailable in this build.");
        return;
    }
    static const FileFilter filters[] = {{"Settings", "json"}};
    FilePickOptions options = FILE_PICK_OPTIONS_INIT;
    options.filters = filters;
    options.filter_count = 1;
    const ModResult result = svc_file->pick_file(ctx, &options, on_import_picked, nullptr);
    if (result != MOD_OK) {
        show_error("Settings import failed", result == MOD_CONFLICT ?
                                                 "Another file dialog is already open." :
                                                 "The file picker could not be opened.");
    }
}

void on_transfer_pressed(ModContext* ctx, void*) {
    if (svc_ui == nullptr || g_button == 0) return;

    UiContextMenuItem items[2] = {UI_CONTEXT_MENU_ITEM_INIT, UI_CONTEXT_MENU_ITEM_INIT};
    items[0].label = "Export settings";
    items[0].icon = "download";
    items[0].on_pressed = on_export_selected;
    items[0].enabled = svc_file != nullptr;

    items[1].label = "Import settings";
    items[1].icon = "folder_open";
    items[1].on_pressed = on_import_selected;
    items[1].enabled = svc_file != nullptr && g_base != nullptr;

    UiContextMenuDesc desc = UI_CONTEXT_MENU_DESC_INIT;
    desc.items = items;
    desc.item_count = 2;

    UiContextMenuHandle menu = 0;
    svc_ui->context_menu_push(ctx, g_button, &desc, &menu);
}

}

void install_tracking(const ConfigService** service) {
    g_vars.clear();
    if (service == nullptr || *service == nullptr || *service == &g_tracking) return;
    g_base = *service;
    g_tracking = ConfigService{};
    const size_t hostSize = g_base->header.struct_size;
    std::memcpy(&g_tracking, g_base, hostSize < sizeof(g_tracking) ? hostSize : sizeof(g_tracking));
    g_tracking.register_var = tracking_register_var;
    g_tracking.unregister_var = tracking_unregister_var;
    *service = &g_tracking;
}

void uninstall_tracking(const ConfigService** service) {
    g_vars.clear();
    remove_export_temp();
    if (service != nullptr && *service == &g_tracking && g_base != nullptr) {
        *service = g_base;
    }
}

std::string export_json() {
    std::vector<const TrackedVar*> vars;
    for (const TrackedVar& var : g_vars) {
        if (!is_excluded(var.name)) vars.push_back(&var);
    }
    std::sort(vars.begin(), vars.end(),
        [](const TrackedVar* a, const TrackedVar* b) { return a->name < b->name; });

    std::string out = "{\n";
    out += "  \"settings\": {";
    bool first = true;
    for (const TrackedVar* var : vars) {
        std::string value;
        if (!read_value(*var, &value)) continue;
        out += first ? "\n" : ",\n";
        first = false;
        out += "    \"" + json_escape(var->name) + "\": " + value;
    }
    out += first ? "}\n" : "\n  }\n";
    out += "}\n";
    return out;
}

void add_transfer_button(ModContext* ctx, UiElementHandle pane) {
    g_button = 0;
    if (svc_ui == nullptr) return;
    UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
    ctrl.kind = UI_CONTROL_BUTTON;
    ctrl.label = "Export / import settings";
    ctrl.on_pressed = on_transfer_pressed;
    svc_ui->pane_add_control(ctx, pane, &ctrl, &g_button);
}

}

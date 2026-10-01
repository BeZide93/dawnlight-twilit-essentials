#include "bug_report.hpp"
#include "settings_transfer.hpp"

#include "mods/svc/host.h"
#include "mods/svc/http.h"
#include "mods/svc/http.hpp"
#include "mods/svc/log.h"
#include "mods/svc/ui.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <vector>

extern const HostService* svc_host;
extern const HttpService* svc_http;
extern const LogService* svc_log;
extern const UiService* svc_ui;

namespace bug_report {
namespace {

constexpr uint8_t kWebhookKey[] = {
    0x23, 0x41, 0xEE, 0x10, 0x25, 0x5E, 0xFF, 0xE8, 0xEB, 0xB9, 0x67, 0x6B, 0x6E, 0x60, 0x15, 0x39,
};

constexpr uint8_t kEncodedWebhook[] = {
    0x4B, 0x35, 0x9A, 0x60, 0x56, 0x64, 0xD0, 0xC7, 0x8F, 0xD0, 0x14, 0x08, 0x01, 0x12, 0x71, 0x17,
    0x40, 0x2E, 0x83, 0x3F, 0x44, 0x2E, 0x96, 0xC7, 0x9C, 0xDC, 0x05, 0x03, 0x01, 0x0F, 0x7E, 0x4A,
    0x0C, 0x70, 0xDB, 0x25, 0x11, 0x68, 0xCE, 0xDB, 0xD9, 0x8E, 0x57, 0x5F, 0x5B, 0x53, 0x2C, 0x0D,
    0x17, 0x72, 0xDA, 0x21, 0x0A, 0x1A, 0xCB, 0x81, 0x93, 0xD2, 0x24, 0x5A, 0x14, 0x15, 0x23, 0x7F,
    0x13, 0x2A, 0x94, 0x57, 0x5D, 0x19, 0x89, 0x9B, 0xA8, 0xDC, 0x20, 0x1E, 0x0A, 0x38, 0x38, 0x4E,
    0x48, 0x14, 0xAF, 0x61, 0x69, 0x2D, 0x9C, 0xC5, 0xB9, 0xC0, 0x25, 0x2C, 0x0D, 0x3A, 0x5A, 0x61,
    0x12, 0x18, 0x88, 0x44, 0x4F, 0x04, 0xAB, 0x9C, 0x83, 0xDF, 0x35, 0x22, 0x01, 0x53, 0x74, 0x49,
    0x6C, 0x18, 0x8B, 0x63, 0x16, 0x29, 0x9E, 0x98, 0xDE,
};

constexpr const char* kHistoryFileName = "bug_reports.txt";

constexpr size_t kMaxLogBytes = 2u * 1024u * 1024u;
constexpr size_t kMaxConfigBytes = 1024u * 1024u;
constexpr size_t kMaxModListBytes = 256u * 1024u;
constexpr size_t kMaxSaveBytes = 8u * 1024u * 1024u;
constexpr size_t kMaxLogCount = 3;
constexpr int32_t kMaxDiscordNameLength = 32;
constexpr int32_t kMaxDescriptionLength = 1500;

constexpr size_t kCardBlockSize = 0x2000;
constexpr size_t kMaxCardBlocks = 0x800;
constexpr size_t kGciDirEntries = 127;
constexpr size_t kGciEntrySize = 0x40;

struct Attachment {
    std::string filename;
    std::string mime;
    std::vector<uint8_t> data;
};

struct Report {
    std::string content;
    std::vector<Attachment> attachments;
};

std::string g_pendingId;
std::string g_sentId;
bool g_sendInFlight = false;
std::string g_discordName;
std::string g_description;

std::string decode_webhook_url() {
    std::string url;
    url.reserve(sizeof(kEncodedWebhook));
    volatile const uint8_t* encoded = kEncodedWebhook;
    volatile const uint8_t* key = kWebhookKey;
    for (size_t i = 0; i < sizeof(kEncodedWebhook); ++i) {
        url += static_cast<char>(encoded[i] ^ key[i % sizeof(kWebhookKey)]);
    }
    return url;
}

const char* platform_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm value{};
#if defined(_WIN32)
    gmtime_s(&value, &now);
#else
    gmtime_r(&now, &value);
#endif
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%SZ", &value);
    return buffer;
}

std::string to_lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string trim(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string normalize_for_match(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

std::filesystem::path resolve_data_root() {
    if (svc_host != nullptr && mod_ctx != nullptr) {
        const char* dir = nullptr;
        if (svc_host->data_dir(mod_ctx, &dir) == MOD_OK && dir != nullptr && dir[0] != '\0') {
            const std::filesystem::path dataDir(dir);
            const std::filesystem::path parent = dataDir.parent_path();
            if (parent.filename() == "mod_data") {
                return parent.parent_path();
            }
        }
    }

#if defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr && appData[0] != '\0') {
        return std::filesystem::path(appData) / "TwilitRealm" / "Dusklight";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / "Library" / "Application Support" / "TwilitRealm" /
               "Dusklight";
    }
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::filesystem::path base;
    if (xdg != nullptr && xdg[0] != '\0') {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        base = std::filesystem::path(home) / ".local" / "share";
    }
    if (!base.empty()) {
        return base / "TwilitRealm" / "Dusklight";
    }
#endif
    return {};
}

bool newest_matching_file(const std::filesystem::path& folder, const char* extension,
    const char* preferredPrefix, std::filesystem::path* out) {
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) return false;

    std::filesystem::path best;
    bool bestPreferred = false;
    auto bestTime = std::filesystem::file_time_type::min();

    std::filesystem::directory_iterator it(folder,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return false;
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const std::filesystem::directory_entry& entry = *it;
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || entryEc) continue;
        if (extension != nullptr && entry.path().extension().string() != extension) continue;

        const std::string name = entry.path().filename().string();
        const bool preferred = preferredPrefix == nullptr ||
                               to_lower(name).rfind(preferredPrefix, 0) == 0;
        auto modified = entry.last_write_time(entryEc);
        if (entryEc) continue;

        const bool better = preferred && !bestPreferred ? true :
                            preferred != bestPreferred  ? false :
                                                          modified > bestTime;
        if (better || best.empty()) {
            best = entry.path();
            bestPreferred = preferred;
            bestTime = modified;
        }
    }
    if (best.empty()) return false;
    *out = best;
    return true;
}

bool read_file_bytes(
    const std::filesystem::path& path, size_t maxBytes, bool keepTail, Attachment* out) {
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr) return false;

    bool ok = false;
    do {
        if (std::fseek(file, 0, SEEK_END) != 0) break;
        const long fileSize = std::ftell(file);
        if (fileSize < 0) break;

        long offset = 0;
        if (static_cast<size_t>(fileSize) > maxBytes) {
            offset = keepTail ? static_cast<long>(static_cast<size_t>(fileSize) - maxBytes) : 0;
        }
        const size_t readSize = static_cast<size_t>(fileSize - offset);

        if (std::fseek(file, offset, SEEK_SET) != 0) break;
        out->data.resize(readSize);
        if (readSize != 0 &&
            std::fread(out->data.data(), 1, readSize, file) != readSize) {
            break;
        }
        ok = true;
    } while (false);
    std::fclose(file);
    return ok;
}

bool read_text_file(const std::filesystem::path& path, size_t maxBytes, std::string* out) {
    Attachment data;
    if (!read_file_bytes(path, maxBytes, true, &data)) return false;
    out->assign(reinterpret_cast<const char*>(data.data.data()), data.data.size());
    return true;
}

uint16_t read_u16be(const uint8_t* data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

uint32_t read_u32be(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

bool directory_checksum_ok(const uint8_t* block) {
    uint16_t sum = 0;
    uint16_t inv = 0;
    for (uint32_t offset = 0; offset < 0x1FF8u; offset += 2) {
        const uint16_t value = read_u16be(block + offset);
        sum = static_cast<uint16_t>(sum + value);
        inv = static_cast<uint16_t>(inv + (value ^ 0xFFFFu));
    }
    return sum == read_u16be(block + 0x1FFC) && inv == read_u16be(block + 0x1FFE);
}

const uint8_t* pick_directory(const uint8_t* raw, size_t size) {
    if (size < 4 * kCardBlockSize) return nullptr;
    const uint8_t* copies[2] = {raw + kCardBlockSize, raw + 2 * kCardBlockSize};
    const bool ok[2] = {directory_checksum_ok(copies[0]), directory_checksum_ok(copies[1])};
    if (ok[0] && ok[1]) {
        return read_u16be(copies[0] + 0x1FF8) >= read_u16be(copies[1] + 0x1FF8) ? copies[0] :
                                                                                 copies[1];
    }
    if (ok[0]) return copies[0];
    if (ok[1]) return copies[1];
    return nullptr;
}

bool extract_gci_from_raw(const std::filesystem::path& rawPath, Attachment* out) {
    Attachment card;
    if (!read_file_bytes(rawPath, 32u * 1024u * 1024u, false, &card)) return false;

    const uint8_t* raw = card.data.data();
    const size_t size = card.data.size();
    const size_t totalBlocks = size / kCardBlockSize;
    if (totalBlocks < 5 + 1 || totalBlocks > kMaxCardBlocks + 5) return false;

    const uint8_t* directory = pick_directory(raw, size);
    if (directory == nullptr) return false;
    const uint8_t* bat = raw + 3 * kCardBlockSize;

    const uint8_t* bestEntry = nullptr;
    bool bestPreferred = false;
    uint32_t bestTime = 0;
    for (size_t entry = 0; entry < kGciDirEntries; ++entry) {
        const uint8_t* base = directory + entry * kGciEntrySize;
        bool free = true;
        for (size_t i = 0; i < 0x07; ++i) {
            if (base[i] != 0xFF) {
                free = false;
                break;
            }
        }
        if (free) continue;

        const uint16_t firstBlock = read_u16be(base + 0x36);
        const uint16_t blockCount = read_u16be(base + 0x38);
        if (blockCount == 0 || blockCount > kMaxCardBlocks) continue;
        if (firstBlock == 0 || firstBlock == 0xFFFF) continue;
        if (firstBlock + blockCount > totalBlocks - 5) continue;

        const bool preferred = std::memcmp(base, "GZ2", 3) == 0;
        const uint32_t modified = read_u32be(base + 0x28);
        if (bestEntry == nullptr || (preferred && !bestPreferred) ||
            (preferred == bestPreferred && modified > bestTime)) {
            bestEntry = base;
            bestPreferred = preferred;
            bestTime = modified;
        }
    }
    if (bestEntry == nullptr) return false;

    const uint16_t firstBlock = read_u16be(bestEntry + 0x36);
    const uint16_t blockCount = read_u16be(bestEntry + 0x38);

    out->data.reserve(0x80 + blockCount * kCardBlockSize);
    out->data.insert(out->data.end(), bestEntry, bestEntry + kGciEntrySize);
    out->data.insert(out->data.end(), kGciEntrySize, 0x00);

    uint16_t index = firstBlock;
    for (uint16_t block = 0; block < blockCount; ++block) {
        if (index == 0 || index == 0xFFFF || index >= 0xFFB || index + 5 >= totalBlocks) {
            return false;
        }
        const uint8_t* data = raw + (index + 5) * kCardBlockSize;
        out->data.insert(out->data.end(), data, data + kCardBlockSize);
        if (block + 1 < blockCount) {
            index = read_u16be(bat + 0x0A + index * 2);
        }
    }
    return true;
}

void collect_logs(const std::filesystem::path& root, Report* report) {
    const std::filesystem::path logsDir = root / "logs";
    std::error_code ec;
    if (!std::filesystem::is_directory(logsDir, ec)) return;

    struct LogCandidate {
        std::filesystem::path path;
        std::filesystem::file_time_type mtime;
    };
    std::vector<LogCandidate> logs;
    std::filesystem::directory_iterator it(logsDir,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return;
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const std::filesystem::directory_entry& entry = *it;
        std::error_code entryEc;
        if (!entry.is_regular_file(entryEc) || entryEc) continue;
        if (entry.path().extension().string() != ".log") continue;
        auto modified = entry.last_write_time(entryEc);
        if (entryEc) continue;
        logs.push_back({entry.path(), modified});
    }

    std::sort(logs.begin(), logs.end(), [](const LogCandidate& a, const LogCandidate& b) {
        return a.mtime > b.mtime;
    });
    if (logs.size() > kMaxLogCount) {
        logs.resize(kMaxLogCount);
    }

    for (const LogCandidate& log : logs) {
        Attachment attachment;
        if (!read_file_bytes(log.path, kMaxLogBytes, true, &attachment)) continue;
        attachment.filename = log.path.filename().string();
        attachment.mime = "text/plain";
        report->attachments.push_back(std::move(attachment));
    }
}

bool extract_json_string(std::string_view json, std::string_view key, std::string* out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string_view::npos) return false;
    pos = json.find('"', pos);
    if (pos == std::string_view::npos) return false;
    ++pos;
    std::string value;
    for (; pos < json.size(); ++pos) {
        const char c = json[pos];
        if (c == '"') {
            *out = std::move(value);
            return true;
        }
        if (c == '\\' && pos + 1 < json.size()) {
            ++pos;
            if (json[pos] == 'n' || json[pos] == 't') value += ' ';
            continue;
        }
        value += c;
    }
    return false;
}

std::string unescape_mod_id(std::string_view escaped) {
    std::string id;
    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '_') {
            if (i + 1 < escaped.size() && escaped[i + 1] == '_') {
                id += '_';
                ++i;
            } else {
                id += '.';
            }
        } else {
            id += escaped[i];
        }
    }
    return id;
}

struct ModListEntry {
    std::string id;
    int enabled = -1;
    std::string source;
    std::string version;
};

bool collect_mod_list(const std::filesystem::path& root, Attachment* out) {
    const std::filesystem::path modsDir = root / "mods";
    std::error_code ec;
    if (!std::filesystem::is_directory(modsDir, ec)) return false;

    std::string configText;
    read_text_file(root / "config.json", kMaxConfigBytes, &configText);

    std::vector<ModListEntry> entries;
    size_t pos = 0;
    while ((pos = configText.find("\"mod.", pos)) != std::string::npos) {
        const size_t keyEnd = configText.find('"', pos + 1);
        if (keyEnd == std::string::npos) break;
        const std::string key = configText.substr(pos + 1, keyEnd - pos - 1);
        pos = keyEnd;
        if (key.size() <= 12 || key.rfind(".enabled") != key.size() - 8) continue;

        const std::string id = unescape_mod_id(key.substr(4, key.size() - 12));
        if (id.empty()) continue;

        const size_t colon = configText.find(':', keyEnd);
        if (colon == std::string::npos) break;
        size_t value = colon + 1;
        while (value < configText.size() &&
               std::isspace(static_cast<unsigned char>(configText[value]))) {
            ++value;
        }
        const bool enabled = configText.compare(value, 4, "true") == 0;

        auto existing = std::find_if(entries.begin(), entries.end(),
            [&id](const ModListEntry& entry) { return entry.id == id; });
        if (existing != entries.end()) {
            existing->enabled = enabled ? 1 : 0;
        } else {
            entries.push_back({id, enabled ? 1 : 0, {}, {}});
        }
    }

    std::filesystem::directory_iterator it(modsDir,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return false;
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const std::filesystem::directory_entry& item = *it;
        std::error_code itemEc;
        const std::string itemName = item.path().filename().string();
        if (!itemName.empty() && itemName[0] == '.') continue;
        if (item.is_directory(itemEc) && !itemEc) {
            std::string id;
            std::string version;
            std::string json;
            if (read_text_file(item.path() / "mod.json", 16u * 1024u, &json)) {
                extract_json_string(json, "id", &id);
                extract_json_string(json, "version", &version);
            }
            if (id.empty()) {
                continue;
            }
            auto existing = std::find_if(entries.begin(), entries.end(),
                [&id](const ModListEntry& entry) { return entry.id == id; });
            if (existing != entries.end()) {
                existing->source = itemName;
                existing->version = version;
            } else {
                entries.push_back({id, -1, itemName, version});
            }
            continue;
        }

        const std::string name = itemName;
        if (name.size() < 5 || name.rfind(".dusk") != name.size() - 5) continue;
        const std::string stem = name.substr(0, name.size() - 5);
        const std::string normStem = normalize_for_match(stem);
        auto existing = std::find_if(entries.begin(), entries.end(),
            [&normStem](const ModListEntry& entry) {
                return normalize_for_match(entry.id) == normStem;
            });
        if (existing != entries.end() && existing->source.empty()) {
            existing->source = name;
        }
    }

    std::vector<ModListEntry> listed;
    for (const ModListEntry& entry : entries) {
        if (configText.empty() || entry.enabled == 1) {
            listed.push_back(entry);
        }
    }

    std::sort(listed.begin(), listed.end(), [](const ModListEntry& a, const ModListEntry& b) {
        return a.id < b.id;
    });

    std::string text = configText.empty() ? "# Mods (config.json unreadable, folder scan)\n" :
                                            "# Enabled mods (from config.json)\n";
    for (const ModListEntry& entry : listed) {
        text += entry.id;
        if (!entry.version.empty()) text += " | v" + entry.version;
        if (!entry.source.empty()) text += " | " + entry.source;
        text += "\n";
    }
    if (listed.empty()) {
        text += "(none)\n";
    }
    if (text.size() > kMaxModListBytes) {
        text.resize(kMaxModListBytes);
    }

    out->filename = "mod_list.txt";
    out->mime = "text/plain";
    out->data.assign(text.begin(), text.end());
    return true;
}

void scan_gci_candidates(const std::filesystem::path& base,
    std::vector<std::filesystem::path>* gciDirs, std::vector<std::filesystem::path>* rawFiles) {
    std::error_code ec;
    const std::filesystem::path gcDir = base / "GC";
    if (std::filesystem::is_directory(gcDir, ec)) {
        std::filesystem::directory_iterator region(gcDir,
            std::filesystem::directory_options::skip_permission_denied, ec);
        if (!ec) {
            std::filesystem::directory_iterator regionsEnd;
            for (; region != regionsEnd; region.increment(ec)) {
                if (ec) break;
                const std::filesystem::directory_entry& regionEntry = *region;
                std::error_code regionEc;
                if (!regionEntry.is_directory(regionEc) || regionEc) continue;
                for (const char* card : {"Card A", "Card B"}) {
                    std::error_code cardEc;
                    if (std::filesystem::is_directory(regionEntry.path() / card, cardEc)) {
                        gciDirs->push_back(regionEntry.path() / card);
                    }
                }
                std::filesystem::directory_iterator files(regionEntry.path(),
                    std::filesystem::directory_options::skip_permission_denied, regionEc);
                if (!regionEc) {
                    std::filesystem::directory_iterator filesEnd;
                    for (; files != filesEnd; files.increment(regionEc)) {
                        if (regionEc) break;
                        const std::filesystem::directory_entry& file = *files;
                        std::error_code fileEc;
                        if (file.is_regular_file(fileEc) && !fileEc &&
                            file.path().extension().string() == ".raw") {
                            rawFiles->push_back(file.path());
                        }
                    }
                }
            }
        }
    }
    std::filesystem::directory_iterator loose(base,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (!ec) {
        std::filesystem::directory_iterator looseEnd;
        for (; loose != looseEnd; loose.increment(ec)) {
            if (ec) break;
            const std::filesystem::directory_entry& entry = *loose;
            std::error_code entryEc;
            if (!entry.is_regular_file(entryEc) || entryEc) continue;
            const std::string name = entry.path().filename().string();
            if (name.size() >= 4 && name.rfind(".gci") == name.size() - 4) {
                gciDirs->push_back(entry.path());
            } else if (name.rfind("MemoryCard", 0) == 0 &&
                       name.rfind(".raw") == name.size() - 4) {
                rawFiles->push_back(entry.path());
            }
        }
    }
}

bool collect_save(const std::filesystem::path& root, Attachment* out) {
    std::error_code ec;
    std::vector<std::filesystem::path> gciDirs;
    std::vector<std::filesystem::path> rawFiles;

    std::vector<std::filesystem::path> bases{root};
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) bases.push_back(cwd);
#if defined(_WIN32)
    if (const char* profile = std::getenv("USERPROFILE"); profile != nullptr) {
        bases.push_back(std::filesystem::path(profile) / "Documents" / "Dolphin Emulator");
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        bases.push_back(std::filesystem::path(home) / "Library" / "Application Support" /
                        "Dolphin");
    }
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        bases.push_back(xdg != nullptr && xdg[0] != '\0' ?
                            std::filesystem::path(xdg) / "dolphin-emu" :
                            std::filesystem::path(home) / ".local" / "share" / "dolphin-emu");
        bases.push_back(std::filesystem::path(home) / ".dolphin-emu");
    }
#endif

    for (const std::filesystem::path& base : bases) {
        if (base.empty()) continue;
        scan_gci_candidates(base, &gciDirs, &rawFiles);
    }

    std::filesystem::path best;
    bool bestPreferred = false;
    auto bestTime = std::filesystem::file_time_type::min();
    for (const std::filesystem::path& dir : gciDirs) {
        std::filesystem::path candidate;
        if (!newest_matching_file(dir, ".gci", nullptr, &candidate)) continue;
        const bool preferred = to_lower(candidate.filename().string())
                                   .find("gz2") != std::string::npos;
        std::error_code fileEc;
        auto modified = std::filesystem::last_write_time(candidate, fileEc);
        if (fileEc) continue;
        if (best.empty() || (preferred && !bestPreferred) ||
            (preferred == bestPreferred && modified > bestTime)) {
            best = candidate;
            bestPreferred = preferred;
            bestTime = modified;
        }
    }
    if (!best.empty()) {
        out->filename = best.filename().string();
        out->mime = "application/octet-stream";
        return read_file_bytes(best, kMaxSaveBytes, false, out);
    }

    std::filesystem::path newestRaw;
    for (const std::filesystem::path& raw : rawFiles) {
        std::error_code fileEc;
        auto modified = std::filesystem::last_write_time(raw, fileEc);
        if (fileEc) continue;
        if (newestRaw.empty() || modified > bestTime) {
            newestRaw = raw;
            bestTime = modified;
        }
    }
    if (!newestRaw.empty()) {
        if (extract_gci_from_raw(newestRaw, out)) {
            out->filename = "save_from_raw.gci";
            out->mime = "application/octet-stream";
            return true;
        }
        out->filename = newestRaw.filename().string();
        out->mime = "application/octet-stream";
        return read_file_bytes(newestRaw, kMaxSaveBytes, false, out);
    }
    return false;
}

bool collect_settings_export(const std::string& id, Attachment* out) {
    const std::string json = settings_transfer::export_json();
    out->filename = "te_settings_" + id + ".json";
    out->mime = "application/json";
    out->data.assign(json.begin(), json.end());
    return true;
}

void build_report(Report* report, const std::string& id) {
    const std::filesystem::path root = resolve_data_root();

    std::string lines;
    lines += "**Bug report `" + id + "`**\n";
    if (svc_host != nullptr && mod_ctx != nullptr) {
        lines += std::string(svc_host->mod_name(mod_ctx)) + " " +
                 svc_host->mod_version(mod_ctx) + " (" + svc_host->mod_id(mod_ctx) + ")\n";
        lines += "Dusklight: " + std::string(svc_host->version) + "\n";
    }
    lines += std::string("Platform: ") + platform_name() + "\n";
    lines += "Time: " + utc_timestamp() + "\n";
    const std::string discordName = trim(g_discordName);
    lines += "Discord: " + (discordName.empty() ? std::string("not provided") : discordName) + "\n";
    if (root.empty()) {
        lines += "Data folder: could not be resolved\n";
    }
    const std::string description = trim(g_description);
    lines += "\n" + (description.empty() ? std::string("No description provided.") : description);

    report->content = std::move(lines);

    collect_logs(root, report);
    Attachment attachment;
    if (collect_mod_list(root, &attachment)) {
        report->attachments.push_back(std::move(attachment));
    }
    attachment = Attachment{};
    if (collect_save(root, &attachment)) report->attachments.push_back(std::move(attachment));
    attachment = Attachment{};
    if (collect_settings_export(id, &attachment)) report->attachments.push_back(std::move(attachment));
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

std::string sanitize_filename(std::string name) {
    for (char& c : name) {
        if (c == '"' || c == '\\' || c == '\r' || c == '\n') c = '_';
    }
    return name.empty() ? "attachment.bin" : name;
}

std::string report_boundary(const std::string& id) {
    return "dusklight" + id + "BugReportBoundary";
}

std::string build_multipart(const std::string& id, const Report& report) {
    const std::string boundary = report_boundary(id);
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
    body += "Content-Type: application/json\r\n\r\n";
    body += "{\"content\":\"" + json_escape(report.content) + "\"";
    body += ",\"allowed_mentions\":{\"parse\":[]}";
    body += "}";

    int index = 0;
    for (const Attachment& attachment : report.attachments) {
        body += "\r\n--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"files[" + std::to_string(index++) +
                "]\"; filename=\"" + sanitize_filename(attachment.filename) + "\"\r\n";
        body += "Content-Type: " + attachment.mime + "\r\n\r\n";
        body.append(reinterpret_cast<const char*>(attachment.data.data()),
            attachment.data.size());
    }
    body += "\r\n--" + boundary + "--\r\n";
    return body;
}

std::string generate_report_id() {
    static std::random_device device;
    std::mt19937_64 generator(device() ^
                              (static_cast<uint64_t>(std::time(nullptr)) << 32) ^ device());
    std::uniform_int_distribution<int> digit(0, 15);
    std::string id;
    for (int i = 0; i < 8; ++i) {
        const int value = digit(generator);
        id += static_cast<char>(value < 10 ? '0' + value : 'A' + value - 10);
    }
    return id;
}

std::filesystem::path history_path() {
    if (svc_host == nullptr || mod_ctx == nullptr) return {};
    const char* dir = nullptr;
    if (svc_host->data_dir(mod_ctx, &dir) != MOD_OK || dir == nullptr || dir[0] == '\0') {
        return {};
    }
    return std::filesystem::path(dir) / kHistoryFileName;
}

void append_report_history(const std::string& id) {
    const std::filesystem::path path = history_path();
    if (path.empty()) return;
    std::FILE* file = std::fopen(path.string().c_str(), "ab");
    if (file == nullptr) return;
    std::fprintf(file, "%s\t%s\n", id.c_str(), utc_timestamp().c_str());
    std::fclose(file);
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

void on_copy_id_pressed(ModContext* ctx, UiDialogHandle, void*) {
    if (svc_ui != nullptr) {
        svc_ui->set_clipboard_text(ctx, g_sentId.c_str());
    }
    push_toast("Report ID copied to clipboard.");
}

void show_success_dialog(const std::string& id) {
    if (svc_ui == nullptr) return;
    g_sentId = id;

    static UiDialogAction actions[2];
    actions[0].struct_size = sizeof(UiDialogAction);
    actions[0].label = "Copy ID";
    actions[0].on_pressed = on_copy_id_pressed;
    actions[0].user_data = nullptr;
    actions[0].keep_open = true;

    actions[1].struct_size = sizeof(UiDialogAction);
    actions[1].label = "Close";
    actions[1].on_pressed = nullptr;
    actions[1].user_data = nullptr;
    actions[1].keep_open = false;

    static std::string body;
    body = "<p>Your bug report has been sent. Thank you!</p>"
           "<p>Report ID: <b>" +
           rml_escape(id) +
           "</b></p>"
           "<p>If the problem persists, mention this ID in the Twilit Essentials Discord "
           "channel so your report can be found.</p>";

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Bug report sent";
    desc.body_rml = body.c_str();
    desc.variant = UI_DIALOG_NORMAL;
    desc.icon = "celebration";
    desc.actions = actions;
    desc.action_count = 2;

    UiDialogHandle handle = 0;
    svc_ui->dialog_push(mod_ctx, &desc, &handle);
}

void show_error_dialog(const std::string& detail) {
    if (svc_ui == nullptr) return;

    static UiDialogAction action;
    action.struct_size = sizeof(UiDialogAction);
    action.label = "Close";
    action.on_pressed = nullptr;
    action.user_data = nullptr;
    action.keep_open = false;

    static std::string body;
    body = "<p>The bug report could not be sent.</p><p>" + rml_escape(detail) + "</p>";

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Bug report failed";
    desc.body_rml = body.c_str();
    desc.variant = UI_DIALOG_DANGER;
    desc.actions = &action;
    desc.action_count = 1;

    UiDialogHandle handle = 0;
    svc_ui->dialog_push(mod_ctx, &desc, &handle);
}

void on_webhook_response(mods::http::Response response) {
    g_sendInFlight = false;
    if (response.ok()) {
        append_report_history(g_pendingId);
        show_success_dialog(g_pendingId);
        return;
    }

    std::string detail;
    if (!response.errorMessage.empty()) detail = response.errorMessage;
    if (response.statusCode != 0) {
        if (!detail.empty()) detail += " ";
        detail += "(HTTP " + std::to_string(response.statusCode) + ")";
    }
    if (detail.empty()) detail = "Unknown network error.";
    show_error_dialog(detail);
}

bool send_report() {
    if (svc_http == nullptr) {
        show_error_dialog("Networking is unavailable in this build.");
        return false;
    }

    const std::string id = generate_report_id();
    g_pendingId = id;

    Report report;
    build_report(&report, id);
    const std::string body = build_multipart(id, report);

    mods::http::Request request;
    request.method = HTTP_METHOD_POST;
    request.url = decode_webhook_url();
    request.headers.push_back({"Content-Type", "multipart/form-data; boundary=" +
                                                   report_boundary(id)});
    request.body = std::move(body);
    request.connectTimeoutMs = 10000;
    request.totalTimeoutMs = 60000;
    request.maxBodyBytes = 1024u * 1024u;

    auto pending = mods::http::request(request, on_webhook_response);
    if (!pending) {
        g_sendInFlight = false;
        show_error_dialog("The request could not be started (HTTP service unavailable).");
        return false;
    }
    pending.detach();
    return true;
}

void on_consent_confirmed(ModContext*, UiDialogHandle, void*) {
    if (g_sendInFlight) return;
    g_sendInFlight = true;
    if (send_report()) {
        push_toast("Sending bug report...");
    }
}

void get_report_text(ModContext*, void* user_data, UiControlValue* out_value) {
    out_value->string_value = static_cast<std::string*>(user_data)->c_str();
}

void set_report_text(ModContext*, void* user_data, const UiControlValue* value) {
    *static_cast<std::string*>(user_data) = value->string_value != nullptr ? value->string_value : "";
}

void add_report_text_input(ModContext* ctx, UiElementHandle pane, const char* label, int32_t maxLength,
    std::string* target) {
    UiControlDesc ctrl = UI_CONTROL_DESC_INIT;
    ctrl.kind = UI_CONTROL_STRING;
    ctrl.label = label;
    ctrl.binding = UI_BINDING_CALLBACKS;
    ctrl.get = get_report_text;
    ctrl.set = set_report_text;
    ctrl.user_data = target;
    ctrl.max_length = maxLength;
    ctrl.string_set_mode = UI_STRING_SET_ON_CHANGE;
    svc_ui->pane_add_control(ctx, pane, &ctrl, nullptr);
}

ModResult build_consent_dialog(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    if (svc_ui == nullptr) return MOD_OK;
    add_report_text_input(ctx, pane, "Discord username (optional)", kMaxDiscordNameLength,
        &g_discordName);
    add_report_text_input(ctx, pane, "Describe the bug", kMaxDescriptionLength, &g_description);
    return MOD_OK;
}

}

void on_create_bug_report_pressed(ModContext* ctx, void*) {
    if (!svc_ui) return;
    if (g_sendInFlight) {
        push_toast("A bug report is already being sent.");
        return;
    }

    static UiDialogAction actions[2];
    actions[0].struct_size = sizeof(UiDialogAction);
    actions[0].label = "Cancel";
    actions[0].on_pressed = nullptr;
    actions[0].user_data = nullptr;
    actions[0].keep_open = false;

    actions[1].struct_size = sizeof(UiDialogAction);
    actions[1].label = "Send report";
    actions[1].on_pressed = on_consent_confirmed;
    actions[1].user_data = nullptr;
    actions[1].keep_open = false;

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Send bug report";
    desc.body_rml =
        "<p>Do you agree to send a bug report to Fimmel? "
        "It will include:</p>"
        "<p>\xe2\x80\x93 The last three log files<br/>"
        "\xe2\x80\x93 Your Twilit Essentials settings<br/>"
        "\xe2\x80\x93 Your current save file<br/>"
        "\xe2\x80\x93 The list of installed mods<br/>"
        "</p>"
        "<p>Your report gets an ID you can use to follow up in the Discord channel if "
        "something is still wrong.</p>";
    desc.variant = UI_DIALOG_WARNING;
    desc.actions = actions;
    desc.action_count = 2;
    desc.build = build_consent_dialog;

    g_description.clear();

    UiDialogHandle handle = 0;
    svc_ui->dialog_push(ctx, &desc, &handle);
}

}

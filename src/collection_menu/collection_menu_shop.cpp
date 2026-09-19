#include "collection_menu_shop.hpp"

#include "d/actor/d_a_shop_item.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_msg_object.h"

#include "f_op/f_op_actor_mng.h"

#include "mods/svc/flow.h"
#include "mods/svc/flow.hpp"
#include "mods/svc/item.h"
#include "mods/svc/log.h"
#include "mods/svc/message.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

extern const ItemService* svc_item;
extern const FlowService* svc_flow;
extern const MessageService* svc_message;
extern ModContext* mod_ctx;

namespace {

constexpr const char* kShopStage = "R_SP01";
constexpr u8 kShopRoom = 1;

constexpr u8 kOrdonShieldItem = dItemNo_WOOD_SHIELD_e;

constexpr u8 kTargetSlotVanillaItem = 0x64;

constexpr u16 kCustomPrice = 150;

constexpr char kDescriptionDe[] =
    "Ah, das Ordon-Schild! Ein handgefertigtes Schild aus Ordon mit dem Wappen "
    "des Dorfes. Stark und zuverlaessig! Fuer nur %d Rubine gehoert es dir!";
constexpr char kDescriptionEn[] =
    "Ah, the Ordon Shield! A hand-carved shield from Ordon Village with the "
    "village crest. Sturdy and reliable! Only %d rupees!";
constexpr char kPromptDe[] = "Das Ordon-Schild fuer %d Rubine. Willst du es kaufen?";
constexpr char kPromptEn[] = "The Ordon Shield for %d rupees. Will you buy it?";
constexpr char kPromptYesDe[] = "Kaufen";
constexpr char kPromptYesEn[] = "Buy it";
constexpr char kPromptNoDe[] = "Lieber nicht.";
constexpr char kPromptNoEn[] = "No, thanks.";
constexpr char kNotEnoughDe[] =
    "Hmm... dafuer hast du nicht genug Rubine. Komm wieder, wenn du mehr hast!";
constexpr char kNotEnoughEn[] =
    "Hmm... you don't have enough rupees for that. Come back when you have more!";

constexpr u16 kQueryRupees = 6;
constexpr u8 kEventStartEvent = 8;
constexpr u8 kEventRemoveRupees = 3;
constexpr u8 kEventShopSelect = 16;


bool s_inShop = false;
bool s_resolverRegistered = false;
bool s_groupNudged = false;
bool in_shop_stage() {
    const char* st = dComIfGp_getStartStageName();
    return st != nullptr && std::strcmp(st, kShopStage) == 0 &&
           dComIfGp_roomControl_getStayNo() == kShopRoom;
}

fopAc_ac_c* s_tags[8];
int s_tagCount = 0;

void* tag_collector(void* i_proc, void*) {
    if (s_tagCount < 8 && ((base_process_class*)i_proc)->name == fpcNm_TAG_SHOPITM_e) {
        s_tags[s_tagCount++] = (fopAc_ac_c*)i_proc;
    }
    return nullptr;
}

bool find_first_shop_tag(fopAc_ac_c** outTag) {
    s_tagCount = 0;
    fopAcM_Search(tag_collector, nullptr);
    *outTag = nullptr;
    if (s_tagCount == 0) return false;

    for (int i = 0; i < s_tagCount; ++i) {
        if ((fopAcM_GetParam(s_tags[i]) & 0xFF) == kTargetSlotVanillaItem) {
            *outTag = s_tags[i];
            return true;
        }
    }
    *outTag = s_tags[0];
    return true;
}

bool resolve_ordon_shield(ModContext*, const ItemCheckInfo*, ItemCheckResolution* out, void*) {
    out->item = kOrdonShieldItem;
    out->display_item = kOrdonShieldItem;
    return true;
}

bool register_shop_resolver(u8 vanillaItem) {
    if (svc_item == nullptr) return false;

    char name[64];
    std::snprintf(name, sizeof(name), "shop:%s:%u:%u", kShopStage, (unsigned)kShopRoom,
                  (unsigned)vanillaItem);

    ItemCheckHandle handle = 0;
    const ModResult r =
        svc_item->set_check_resolver(mod_ctx, name, &resolve_ordon_shield, nullptr, &handle);
    return r == MOD_OK;
}

u8 s_slotVanillaItem = 0;
bool s_dynamicResolverTried = false;

void ensure_shop_resolver(fopAc_ac_c* tag) {
    const u8 vanillaItem = fopAcM_GetParam(tag) & 0xFF;
    if (s_slotVanillaItem == 0) s_slotVanillaItem = vanillaItem;

    if (s_resolverRegistered) return;

    if (s_slotVanillaItem == dItemNo_NONE_e || s_slotVanillaItem == 0) return;
    if (s_slotVanillaItem == kTargetSlotVanillaItem) {
        s_resolverRegistered = true;
        return;
    }
    if (s_dynamicResolverTried) return;
    s_dynamicResolverTried = true;
    s_resolverRegistered = register_shop_resolver(s_slotVanillaItem);
}

struct BmgView {
    const uint8_t* bmg = nullptr;
    const uint8_t* entries = nullptr;
    u16 entryCount = 0;
    u16 entrySize = 0;
    const uint8_t* nodes = nullptr;
    u16 nodeCount = 0;
    const uint8_t* edges = nullptr;
    u16 edgeCount = 0;
};

u16 rd16(const uint8_t* p) {
    return static_cast<u16>((p[0] << 8) | p[1]);
}

u32 rd32(const uint8_t* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
           (static_cast<u32>(p[2]) << 8) | p[3];
}

bool find_section(const uint8_t* bmg, const char tag[4], const uint8_t** out, size_t* outSize) {
    if (bmg == nullptr || std::memcmp(bmg, "MESGbmg1", 8) != 0) return false;
    const u32 sectionCount = rd32(bmg + 0x0C);
    size_t offset = 0x20;
    for (u32 i = 0; i < sectionCount; ++i) {
        const u32 sectionSize = rd32(bmg + offset + 4);
        if (sectionSize < 8) return false;
        if (std::memcmp(bmg + offset, tag, 4) == 0) {
            *out = bmg + offset;
            *outSize = sectionSize;
            return true;
        }
        offset += sectionSize;
    }
    return false;
}

bool parse_bmg(const uint8_t* bmg, BmgView& out) {
    if (bmg == nullptr || std::memcmp(bmg, "MESGbmg1", 8) != 0) return false;
    out.bmg = bmg;

    const uint8_t* sec = nullptr;
    size_t size = 0;
    if (find_section(bmg, "INF1", &sec, &size) && size >= 16) {
        out.entryCount = rd16(sec + 8);
        out.entrySize = rd16(sec + 10);
        if (out.entrySize < 20 || (size_t)out.entryCount * out.entrySize > size - 16) {
            out.entryCount = 0;
        } else {
            out.entries = sec + 16;
        }
    }
    if (find_section(bmg, "FLW1", &sec, &size) && size >= 16) {
        out.nodeCount = rd16(sec + 8);
        out.edgeCount = rd16(sec + 10);
        const size_t nodesSize = (size_t)out.nodeCount * 8;
        const size_t edgesSize = (size_t)out.edgeCount * 2;
        if (nodesSize + edgesSize > size - 16) {
            out.nodeCount = 0;
            out.edgeCount = 0;
        } else {
            out.nodes = sec + 16;
            out.edges = out.nodes + nodesSize;
        }
    }
    return out.nodes != nullptr && out.entries != nullptr;
}

u16 message_id_for_entry(const BmgView& bmg, u16 entryIndex) {
    if (entryIndex >= bmg.entryCount) return 0xFFFF;
    const uint8_t* entry = bmg.entries + (size_t)entryIndex * bmg.entrySize;
    return rd16(entry + 4);
}

u16 edge_target(const BmgView& bmg, u16 edgeIndex) {
    if (edgeIndex >= bmg.edgeCount) return 0xFFFF;
    return rd16(bmg.edges + (size_t)edgeIndex * 2);
}

struct ChainInfo {
    u16 startNode = 0xFFFF;
    u16 descMsg = 0xFFFF;
    u16 promptMsg = 0xFFFF;
    u16 notEnoughMsg = 0xFFFF;
    u16 giveItem = 0xFFFF;
    char log[224] = "";

    u16 descNode = 0xFFFF;
    u16 promptNode = 0xFFFF;
    u16 notEnoughNode = 0xFFFF;

    static constexpr int kMaxPricePatches = 6;
    u16 priceNodes[kMaxPricePatches] = {};
    u16 priceParams[kMaxPricePatches] = {};
    int priceCount = 0;
};

void walk_chain(const BmgView& bmg, u16 start, ChainInfo& out) {
    u16 node = start;
    u16 visited[24];
    u16 visitedCount = 0;
    bool seenRupees = false;
    size_t logPos = 0;

    out.startNode = start;
    logPos += (size_t)std::snprintf(out.log + logPos, sizeof(out.log) - logPos, "n%04x", start);

    while (node != 0xFFFF && node < bmg.nodeCount) {
        bool cycle = false;
        for (u16 i = 0; i < visitedCount; ++i) {
            if (visited[i] == node) {
                cycle = true;
                break;
            }
        }
        if (cycle || visitedCount >= 24) break;
        visited[visitedCount++] = node;

        const uint8_t* data = bmg.nodes + (size_t)node * 8;
        const u8 type = data[0];

        if (type == 1) {
            const u16 msgId = message_id_for_entry(bmg, rd16(data + 2));
            if (out.descMsg == 0xFFFF) {
                out.descMsg = msgId;
                out.descNode = node;
            } else if (seenRupees && out.promptMsg == 0xFFFF) {
                out.promptMsg = msgId;
                out.promptNode = node;
            }
            logPos += (size_t)std::snprintf(out.log + logPos, sizeof(out.log) - logPos,
                                            " msg#%04x", msgId);
            node = rd16(data + 4);
        } else if (type == 2) {
            const u16 query = rd16(data + 2);
            const u16 param = rd16(data + 4);
            const u16 edgeBase = rd16(data + 6);
            logPos += (size_t)std::snprintf(out.log + logPos, sizeof(out.log) - logPos,
                                            " br(q%u,p%u)", (unsigned)query, (unsigned)param);
            if (query == kQueryRupees) {
                if (out.priceCount < ChainInfo::kMaxPricePatches) {
                    out.priceNodes[out.priceCount] = node;
                    out.priceParams[out.priceCount] = param;
                    ++out.priceCount;
                }
                if (!seenRupees) {
                    seenRupees = true;
                    const u16 alt = edge_target(bmg, (u16)(edgeBase + 1));
                    if (alt != 0xFFFF && alt < bmg.nodeCount &&
                        bmg.nodes[(size_t)alt * 8] == 1) {
                        out.notEnoughNode = alt;
                        out.notEnoughMsg =
                            message_id_for_entry(bmg, rd16(bmg.nodes + (size_t)alt * 8 + 2));
                    }
                }
            }
            node = edge_target(bmg, edgeBase);
        } else if (type == 3) {
            const u8 eventId = data[1];
            if (eventId == kEventStartEvent && out.giveItem == 0xFFFF) {
                out.giveItem = rd16(data + 6);
            }
            logPos += (size_t)std::snprintf(out.log + logPos, sizeof(out.log) - logPos,
                                            " ev%u", (unsigned)eventId);
            if (eventId == kEventRemoveRupees &&
                out.priceCount < ChainInfo::kMaxPricePatches) {
                out.priceNodes[out.priceCount] = node;
                out.priceParams[out.priceCount] = rd16(data + 4);
                ++out.priceCount;
            }
            node = edge_target(bmg, rd16(data + 2));
        } else {
            logPos += (size_t)std::snprintf(out.log + logPos, sizeof(out.log) - logPos,
                                            " ?%u", (unsigned)type);
            break;
        }
    }
}

void override_text(u16 group, u16 msgId, const char* de, const char* en) {
    if (msgId == 0xFFFF || svc_message == nullptr) return;

    char bufDe[256];
    char bufEn[256];
    std::snprintf(bufDe, sizeof(bufDe), de, (int)kCustomPrice);
    std::snprintf(bufEn, sizeof(bufEn), en, (int)kCustomPrice);

    const struct {
        MessageLanguage lang;
        const char* text;
    } variants[] = {
        {MESSAGE_LANGUAGE_GERMAN, bufDe},
        {MESSAGE_LANGUAGE_ENGLISH, bufEn},
        {MESSAGE_LANGUAGE_FRENCH, bufEn},
        {MESSAGE_LANGUAGE_SPANISH, bufEn},
        {MESSAGE_LANGUAGE_ITALIAN, bufEn},
    };

    for (const auto& v : variants) {
        mods::flow::MessageBuilder builder{};
        builder.text(v.text);
        const mods::flow::MessageVariant built = builder.build(v.lang);
        MessageOverrideHandle handle = 0;
        const ModResult r = svc_message->override_message(mod_ctx, group, msgId, (u8)v.lang,
                                                          built.text().data(),
                                                          built.text().size(), &handle);
        if (r != MOD_OK) {
        }
    }
}

void override_prompt(u16 group, u16 msgId, const char* de, const char* en, const char* yesDe,
                     const char* yesEn, const char* noDe, const char* noEn) {
    if (msgId == 0xFFFF || svc_message == nullptr) return;

    char bufDe[256];
    char bufEn[256];
    std::snprintf(bufDe, sizeof(bufDe), de, (int)kCustomPrice);
    std::snprintf(bufEn, sizeof(bufEn), en, (int)kCustomPrice);

    const struct {
        MessageLanguage lang;
        const char* text;
        const char* yes;
        const char* no;
    } variants[] = {
        {MESSAGE_LANGUAGE_GERMAN, bufDe, yesDe, noDe},
        {MESSAGE_LANGUAGE_ENGLISH, bufEn, yesEn, noEn},
        {MESSAGE_LANGUAGE_FRENCH, bufEn, yesEn, noEn},
        {MESSAGE_LANGUAGE_SPANISH, bufEn, yesEn, noEn},
        {MESSAGE_LANGUAGE_ITALIAN, bufEn, yesEn, noEn},
    };

    for (const auto& v : variants) {
        mods::flow::MessageBuilder builder{};
        builder.text(v.text).options(v.yes, v.no);
        const mods::flow::MessageVariant built = builder.build(v.lang);
        MessageOverrideHandle handle = 0;
        const ModResult r = svc_message->override_message(mod_ctx, group, msgId, (u8)v.lang,
                                                          built.text().data(),
                                                          built.text().size(), &handle);
        if (r != MOD_OK) {
        }
    }
}

bool patch_flow_param(u16 group, u16 nodeIdx, const BmgView& bmg, u16 newParam) {
    if (svc_flow == nullptr) return false;

    FlowGraphHandle handle = 0;
    if (svc_flow->begin_graph(mod_ctx, group, &handle) != MOD_OK) return false;

    FlowNodeData node{};
    std::memcpy(node.bytes, bmg.nodes + (size_t)nodeIdx * 8, 8);
    node.bytes[4] = (u8)(newParam >> 8);
    node.bytes[5] = (u8)(newParam & 0xFF);

    ModResult r = svc_flow->patch_node(mod_ctx, handle, nodeIdx, &node);
    if (r == MOD_OK) r = svc_flow->commit_graph(mod_ctx, handle);
    if (r != MOD_OK) svc_flow->remove_graph(mod_ctx, handle);
    return r == MOD_OK;
}

std::vector<mods::flow::RegisteredMessage> s_customMessages;
struct CustomTextIds {
    u16 desc = 0xFFFF;
    u16 prompt = 0xFFFF;
    u16 notEnough = 0xFFFF;
};
CustomTextIds s_custom{};

u16 register_custom_message(u16 group, const char* de, const char* en, const char* yesDe,
                            const char* yesEn, const char* noDe, const char* noEn) {
    if (svc_message == nullptr) return 0xFFFF;

    char bufDe[256];
    char bufEn[256];
    std::snprintf(bufDe, sizeof(bufDe), de, (int)kCustomPrice);
    std::snprintf(bufEn, sizeof(bufEn), en, (int)kCustomPrice);

    const MessageLanguage langs[] = {
        MESSAGE_LANGUAGE_GERMAN,   MESSAGE_LANGUAGE_ENGLISH, MESSAGE_LANGUAGE_FRENCH,
        MESSAGE_LANGUAGE_SPANISH,  MESSAGE_LANGUAGE_ITALIAN, MESSAGE_LANGUAGE_JAPANESE,
    };

    std::vector<mods::flow::MessageVariant> variants;
    for (const MessageLanguage lang : langs) {
        const bool german = lang == MESSAGE_LANGUAGE_GERMAN;
        mods::flow::MessageBuilder builder{};
        builder.text(german ? bufDe : bufEn);
        if (yesDe != nullptr) {
            builder.options(german ? yesDe : yesEn, german ? noDe : noEn);
        }
        variants.push_back(builder.build(lang));
    }

    MessageId id = 0xFFFF;
    mods::flow::RegisteredMessage reg = mods::flow::register_message(group, variants);
    if (!reg) return 0xFFFF;
    id = reg.id();
    s_customMessages.push_back(std::move(reg));
    return id;
}

bool patch_message_node(u16 group, u16 nodeIdx, const BmgView& bmg, u16 customMsgId) {
    if (svc_flow == nullptr || customMsgId == 0xFFFF || nodeIdx == 0xFFFF ||
        nodeIdx >= bmg.nodeCount || bmg.nodes[(size_t)nodeIdx * 8] != 1) {
        return false;
    }

    FlowGraphHandle handle = 0;
    if (svc_flow->begin_graph(mod_ctx, group, &handle) != MOD_OK) return false;

    FlowNodeData node{};
    std::memcpy(node.bytes, bmg.nodes + (size_t)nodeIdx * 8, 8);
    node.bytes[2] = (u8)(customMsgId >> 8);
    node.bytes[3] = (u8)(customMsgId & 0xFF);

    ModResult r = svc_flow->patch_node(mod_ctx, handle, nodeIdx, &node);
    if (r == MOD_OK) r = svc_flow->commit_graph(mod_ctx, handle);
    if (r != MOD_OK) svc_flow->remove_graph(mod_ctx, handle);
    return r == MOD_OK;
}

u16 s_tagFlowNode = 0xFFFF;
const uint8_t* s_patchedBmg = nullptr;
bool s_nativeOverridesDone = false;

struct ChainScan {
    char log[224] = "";
    int logPos = 0;
    u16 msgNodes[40];
    u16 msgIds[40];
    int msgCount = 0;
    u16 q6Nodes[8];
    u16 q6Params[8];
    int q6Count = 0;
    u16 chargeNodes[8];
    u16 chargeParams[8];
    int chargeCount = 0;
    bool hasGiveVanilla = false;
};

void log_append(ChainScan& out, const char* fmt, ...) {
    if (out.logPos < 0) out.logPos = 0;
    const int capacity = static_cast<int>(sizeof(out.log)) - 1;
    if (out.logPos >= capacity) return;

    va_list args;
    va_start(args, fmt);
    int n = std::vsnprintf(out.log + out.logPos, sizeof(out.log) - out.logPos, fmt, args);
    va_end(args);
    if (n < 0) return;
    if (out.logPos + n > capacity) n = capacity - out.logPos;
    out.logPos += n;
}

void scan_chain(const BmgView& bmg, u16 start, ChainScan& out) {
    u16 seen[96];
    u16 queue[96];
    int seenCount = 0;
    auto enqueue = [&](u16 n) {
        if (n == 0xFFFF || n >= bmg.nodeCount || seenCount >= 96) return;
        for (int i = 0; i < seenCount; ++i) {
            if (seen[i] == n) return;
        }
        seen[seenCount] = n;
        queue[seenCount] = n;
        ++seenCount;
    };
    enqueue(start);
    log_append(out, "n%04x", start);
    for (int head = 0; head < seenCount; ++head) {
        const u16 node = queue[head];
        const uint8_t* d = bmg.nodes + (size_t)node * 8;
        const u8 type = d[0];

        if (type == 1) {
            const u16 msgId = message_id_for_entry(bmg, rd16(d + 2));
            if (out.msgCount < 40) {
                out.msgNodes[out.msgCount] = node;
                out.msgIds[out.msgCount] = msgId;
                ++out.msgCount;
            }
            log_append(out, " msg#%04x", msgId);
            enqueue(rd16(d + 4));
        } else if (type == 2) {
            const u16 query = rd16(d + 2);
            const u16 param = rd16(d + 4);
            const u16 edgeBase = rd16(d + 6);
            const u8 resultCount = d[1];

            log_append(out, " br(q%u,p%u)", (unsigned)query, (unsigned)param);
            if (query == kQueryRupees && out.q6Count < 8) {
                out.q6Nodes[out.q6Count] = node;
                out.q6Params[out.q6Count] = param;
                ++out.q6Count;
            }
            const u8 edgeLimit = resultCount < 4 ? resultCount : 4;
            for (u8 r = 0; r < edgeLimit; ++r) {
                enqueue(edge_target(bmg, (u16)(edgeBase + r)));
            }
        } else if (type == 3) {
            const u8 eventId = d[1];
            log_append(out, " ev%u", (unsigned)eventId);
            if (eventId == kEventStartEvent) {
                const u16 item = rd16(d + 6);
                if (item == kTargetSlotVanillaItem) out.hasGiveVanilla = true;
            }
            if (eventId == kEventRemoveRupees && out.chargeCount < 8) {
                out.chargeNodes[out.chargeCount] = node;
                out.chargeParams[out.chargeCount] = rd16(d + 4);
                ++out.chargeCount;
            }
            enqueue(edge_target(bmg, rd16(d + 2)));
        }
    }
}

struct ShopSelectNode {
    u16 node = 0xFFFF;
    u8 params[4] = {};
};
ShopSelectNode s_selectNodes[8];

void apply_custom_texts() {
    const uint8_t* cur = (const uint8_t*)dMsgObject_getMsgDtPtr();
    if (cur == s_patchedBmg) return;

    const s16 group = dMsgObject_getGroupID();
    if (group <= 0) return;

    BmgView bmg;
    if (!parse_bmg(cur, bmg)) {
        if (!s_groupNudged) {
            s_groupNudged = true;
            dMsgObject_c::changeGroup(group);
        }
        return;
    }

    u16 selectNodes[8];
    u8 selectParams[8][4];
    int selectCount = 0;
    for (u16 n = 0; n < bmg.nodeCount && selectCount < 8; ++n) {
        const uint8_t* d = bmg.nodes + (size_t)n * 8;
        if (d[0] == 3 && d[1] == kEventShopSelect) {
            selectNodes[selectCount] = n;
            for (int k = 0; k < 4; ++k) selectParams[selectCount][k] = d[4 + k];
            ++selectCount;
        }
    }

    bool applied = false;
    for (int e = 0; e < selectCount && !applied; ++e) {
        for (int pb = 0; pb < 4 && !applied; ++pb) {
            const u16 P = selectParams[e][pb];
            if (P == 0 || P == 0xFF) continue;
            const u16 describeStart = (u16)((P - 1) * 2 + 0x65);
            const u16 buyStart = (u16)((P - 1) * 2 + 0x66);
            if (describeStart >= bmg.nodeCount || buyStart >= bmg.nodeCount) continue;

            ChainScan describe;
            ChainScan buy;
            scan_chain(bmg, describeStart, describe);
            scan_chain(bmg, buyStart, buy);
            if (!buy.hasGiveVanilla && describe.hasGiveVanilla) {
                const ChainScan tmp = describe;
                describe = buy;
                buy = tmp;
            }
            if (!buy.hasGiveVanilla) continue;


            int pricePatched = 0;
            for (int b = 0; b < buy.q6Count; ++b) {
                if (buy.q6Params[b] != kCustomPrice &&
                    patch_flow_param((u16)group, buy.q6Nodes[b], bmg, kCustomPrice)) {
                    ++pricePatched;
                }
            }
            for (int c = 0; c < buy.chargeCount; ++c) {
                if (buy.chargeParams[c] != kCustomPrice &&
                    patch_flow_param((u16)group, buy.chargeNodes[c], bmg, kCustomPrice)) {
                    ++pricePatched;
                }
            }

            if (s_custom.desc == 0xFFFF) {
                s_custom.desc = register_custom_message((u16)group, kDescriptionDe,
                                                        kDescriptionEn, nullptr, nullptr,
                                                        nullptr, nullptr);
                s_custom.prompt = register_custom_message((u16)group, kPromptDe, kPromptEn,
                                                          kPromptYesDe, kPromptYesEn,
                                                          kPromptNoDe, kPromptNoEn);
                s_custom.notEnough = register_custom_message((u16)group, kNotEnoughDe,
                                                             kNotEnoughEn, nullptr, nullptr,
                                                             nullptr, nullptr);
            }

            int promptPatched = 0;
            int notEnoughPatched = 0;
            u16 firstPromptId = 0xFFFF;
            u16 firstNotEnoughId = 0xFFFF;
            for (int i = 0; i < buy.msgCount; ++i) {
                const u16 node = buy.msgNodes[i];
                const uint8_t* nd = bmg.nodes + (size_t)node * 8;
                const u16 next = rd16(nd + 4);

                bool isPrompt = false;
                if (next < bmg.nodeCount) {
                    const uint8_t* nn = bmg.nodes + (size_t)next * 8;
                    if (nn[0] == 2) {
                        const u16 nq = rd16(nn + 2);
                        isPrompt = (nq == 0 || nq == 4 || nq == 35 || nq == 36);
                    }
                }
                if (isPrompt) {
                    if (patch_message_node((u16)group, node, bmg, s_custom.prompt)) {
                        ++promptPatched;
                        if (firstPromptId == 0xFFFF) firstPromptId = buy.msgIds[i];
                    }
                    continue;
                }

                bool isNotEnough = false;
                for (int b = 0; b < buy.q6Count; ++b) {
                    const u16 alt = edge_target(
                        bmg, (u16)(rd16(bmg.nodes + (size_t)buy.q6Nodes[b] * 8 + 6) + 1));
                    if (alt == node) {
                        isNotEnough = true;
                        break;
                    }
                }
                if (isNotEnough) {
                    if (patch_message_node((u16)group, node, bmg, s_custom.notEnough)) {
                        ++notEnoughPatched;
                        if (firstNotEnoughId == 0xFFFF) firstNotEnoughId = buy.msgIds[i];
                    }
                }
            }

            int descPatched = 0;
            for (int i = 0; i < describe.msgCount; ++i) {
                if (patch_message_node((u16)group, describe.msgNodes[i], bmg,
                                       s_custom.desc)) {
                    ++descPatched;
                }
            }

            if (!s_nativeOverridesDone) {
                override_prompt((u16)group, firstPromptId, kPromptDe, kPromptEn,
                                kPromptYesDe, kPromptYesEn, kPromptNoDe, kPromptNoEn);
                override_text((u16)group, firstNotEnoughId, kNotEnoughDe, kNotEnoughEn);
                s_nativeOverridesDone = true;
            }

            s_patchedBmg = cur;
            applied = true;
        }
    }
}

int s_diagDelay = -1;
bool s_diagDone = false;

daShopItem_c* s_shopActors[8] = {};
int s_shopActorCount = 0;

void* shopitem_collector(void* i_proc, void*) {
    if (s_shopActorCount < 8 && ((base_process_class*)i_proc)->name == fpcNm_ShopItem_e) {
        s_shopActors[s_shopActorCount++] = static_cast<daShopItem_c*>(i_proc);
    }
    return nullptr;
}

void collect_display_actors() {
    s_shopActorCount = 0;
    fopAcM_Search(shopitem_collector, nullptr);
}

void run_display_diagnostics() {
    if (!s_resolverRegistered || s_diagDone) return;
    if (s_diagDelay < 0) s_diagDelay = 60;
    if (--s_diagDelay > 0) return;
    s_diagDone = true;

    collect_display_actors();

    if (s_shopActorCount == 0) {
        return;
    }
    for (int i = 0; i < s_shopActorCount; ++i) {
        daShopItem_c* item = s_shopActors[i];
    }
}

}

void update_collection_menu_shop(const LogService*, ModContext*) {
    if (!in_shop_stage()) {
        s_inShop = false;
        s_groupNudged = false;
            return;
    }
    s_inShop = true;

    collect_display_actors();
    run_display_diagnostics();

    fopAc_ac_c* tag = nullptr;
    if (!find_first_shop_tag(&tag)) return;

    if (s_tagFlowNode == 0xFFFF &&
        (fopAcM_GetParam(tag) & 0xFF) == kTargetSlotVanillaItem) {
        s_tagFlowNode = (u16)tag->home.angle.x;
    }
    ensure_shop_resolver(tag);
    apply_custom_texts();
}

ModResult init_collection_menu_shop(const HookService*, const LogService* log_svc,
                                    ModContext* mod_ctx, ModError*) {
    if (register_shop_resolver(kTargetSlotVanillaItem)) {
        s_resolverRegistered = true;
    }
    return MOD_OK;
}

void shutdown_collection_menu_shop() {
    s_inShop = false;
    s_groupNudged = false;
    s_resolverRegistered = false;
}

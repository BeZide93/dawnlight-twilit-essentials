#include "boss_rush_midna.hpp"
#include "boss_rush.hpp"
#include "boss_rush_common.hpp"

#include "mods/svc/flow.hpp"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_midna.h"
#include "d/actor/d_a_tag_mhint.h"
#include "d/d_com_inf_game.h"
#include "d/d_event.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "f_op/f_op_overlap_mng.h"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/JUtility/JUTFader.h"
#include "mods/svc/hook.h"
#include "mods/svc/hook.hpp"

struct BossRushOrderZTalkHook;
DEFINE_HOOK(&daAlink_c::orderZTalk, BossRushOrderZTalkHook);

struct BossRushNotTalkHook;
DEFINE_HOOK(&daAlink_c::notTalk, BossRushNotTalkHook);

extern bool g_dpadLeftTrig;
extern bool g_configCustomZButtonEnabled;

static int s_talkHoldTicks = 0;

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kMessageGroup = 0;
constexpr uint16_t kMidnaSpeaker = 21;
constexpr uint16_t kMidnaPromptHumanNode = 0x018c;
constexpr uint16_t kMidnaPromptWolfNode = 0x018d;
constexpr uint16_t kMidnaHumanBranch = 0x0190;
constexpr uint16_t kMidnaWolfBranch = 0x0193;
constexpr uint16_t kMidnaNoWarpPromptAId = 0x07d3;
constexpr uint16_t kMidnaNoWarpPromptBId = 0x07f6;
constexpr uint16_t kMidnaMenuPromptId = 2042;
constexpr uint16_t kMidnaMenuPromptEntry = 3003;

constexpr std::array kAllLanguages{
    MESSAGE_LANGUAGE_ENGLISH,
    MESSAGE_LANGUAGE_GERMAN,
    MESSAGE_LANGUAGE_FRENCH,
    MESSAGE_LANGUAGE_SPANISH,
    MESSAGE_LANGUAGE_ITALIAN,
    MESSAGE_LANGUAGE_JAPANESE,
};

enum class BossRushMidnaMode { None, Chamber, Fight };
static BossRushMidnaMode s_bossRushMidnaMode = BossRushMidnaMode::None;

enum class MidnaTransformOption {
    Human,
    Wolf,
    Unknown,
};

struct MidnaPromptPatchPoint {
    uint16_t promptNode = mods::flow::kEnd;
    uint16_t promptEntry = 0;
    uint16_t promptMessageId = 0xffff;
    uint16_t transformTarget = mods::flow::kEnd;
    uint16_t secondTarget = mods::flow::kEnd;
    uint16_t thirdTarget = mods::flow::kEnd;
    uint8_t nativeChoiceCount = 0;
    MidnaTransformOption transformOption = MidnaTransformOption::Unknown;

    bool valid() const {
        return promptNode != mods::flow::kEnd && transformTarget != mods::flow::kEnd &&
               secondTarget != mods::flow::kEnd && nativeChoiceCount >= 2;
    }
    bool has_warp_choice() const { return nativeChoiceCount >= 3; }
};

struct MidnaFlowTopology {
    std::vector<MidnaPromptPatchPoint> prompts;
    size_t promptCount = 0;
    const void* resource = nullptr;
    uint32_t version = 0;
};

static MidnaFlowTopology s_bossRushMidnaTopology;
static uint32_t s_bossRushMidnaTopologyVersion = 0;
static MidnaTransformOption s_bossRushMidnaTransformOption = MidnaTransformOption::Unknown;
static bool s_bossRushMidnaHorseback = false;
static int s_bossRushMidnaGauntletPhase = 0;

constexpr uint16_t kMidnaHorseTalkTriggerNode = 0;
static const void* s_bossRushMidnaHorseResource = nullptr;
static uint16_t s_bossRushMidnaHorseNextNode = mods::flow::kEnd;

static uint16_t s_midnaRoot3001 = mods::flow::kEnd;

static uint16_t s_midnaVessel2Way = kMidnaMenuPromptEntry;
static uint16_t s_midnaVessel3Way = kMidnaMenuPromptEntry;

enum class PendingBossRushMidnaAction { None, LeaveChamber, BackToChamber, RetryFight };
static PendingBossRushMidnaAction s_pendingMidnaAction = PendingBossRushMidnaAction::None;
static u8 s_pendingMidnaDelay = 0;

static mods::flow::RegisteredMessage s_bossRushMidnaLeaveOnlyMsg;
static mods::flow::RegisteredMessage s_bossRushMidnaBackOnlyMsg;
static mods::flow::RegisteredMessage s_bossRushMidnaFightBackMsg;
static mods::flow::RegisteredMessage s_bossRushMidnaHumanFightMsg;
static mods::flow::RegisteredMessage s_bossRushMidnaWolfFightMsg;
static mods::flow::Event s_bossRushLeaveMidnaEvent;
static mods::flow::Event s_bossRushRetryMidnaEvent;
static mods::flow::Graph s_bossRushMidnaFlowGraph;


static uint16_t read_be16(const uint8_t* bytes) {
    return mods::read_bits<uint16_t>(bytes);
}

static uint32_t read_be32(const uint8_t* bytes) {
    return mods::read_bits<uint32_t>(bytes);
}

static bool find_bmg_section(const uint8_t* bmg, uint32_t tag, const uint8_t*& outSection,
    size_t& outSize) {
    outSection = nullptr;
    outSize = 0;
    if (bmg == nullptr || std::memcmp(bmg, "MESGbmg1", 8) != 0) {
        return false;
    }

    if (read_be32(bmg + 8) < 0x20) {
        return false;
    }

    const uint32_t sectionCount = read_be32(bmg + 0x0c);
    size_t offset = 0x20;
    for (uint32_t i = 0; i < sectionCount; ++i) {
        if (offset > std::numeric_limits<size_t>::max() - 8) {
            return false;
        }

        const size_t sectionSize = read_be32(bmg + offset + 4);
        if (sectionSize < 8 || offset > std::numeric_limits<size_t>::max() - sectionSize) {
            return false;
        }

        if (read_be32(bmg + offset) == tag) {
            outSection = bmg + offset;
            outSize = sectionSize;
            return true;
        }
        offset += sectionSize;
    }
    return false;
}

struct MidnaBmgView {
    const uint8_t* inf = nullptr;
    size_t infSize = 0;
    uint16_t entryCount = 0;
    uint16_t entrySize = 0;
    const uint8_t* dat = nullptr;
    size_t datSize = 0;
    const uint8_t* flw = nullptr;
    size_t flwSize = 0;
    const uint8_t* nodes = nullptr;
    const uint8_t* edges = nullptr;
    uint16_t nodeCount = 0;
    uint16_t edgeCount = 0;
};

static bool parse_midna_bmg_view(const uint8_t* bmg, MidnaBmgView& out) {
    const uint8_t* section = nullptr;
    size_t sectionSize = 0;
    if (!find_bmg_section(bmg, MULTI_CHAR('INF1'), section, sectionSize) || sectionSize < 16) {
        return false;
    }
    out = {};
    out.inf = section;
    out.infSize = sectionSize;
    out.entryCount = read_be16(section + 8);
    out.entrySize = read_be16(section + 10);
    if (out.entrySize < 20 ||
        static_cast<size_t>(out.entryCount) * out.entrySize > sectionSize - 16)
    {
        return false;
    }

    if (!find_bmg_section(bmg, MULTI_CHAR('DAT1'), section, sectionSize) || sectionSize < 8) {
        return false;
    }
    out.dat = section;
    out.datSize = sectionSize;

    if (!find_bmg_section(bmg, MULTI_CHAR('FLW1'), section, sectionSize) || sectionSize < 16) {
        return false;
    }
    out.flw = section;
    out.flwSize = sectionSize;
    out.nodeCount = read_be16(section + 8);
    out.edgeCount = read_be16(section + 10);
    const size_t nodeBytes = static_cast<size_t>(out.nodeCount) * 8;
    const size_t edgeBytes = static_cast<size_t>(out.edgeCount) * 2;
    if (nodeBytes > sectionSize - 16 || edgeBytes > sectionSize - 16 - nodeBytes) {
        return false;
    }
    out.nodes = section + 16;
    out.edges = out.nodes + nodeBytes;

    return true;
}

static const uint8_t* bmg_entry(const MidnaBmgView& view, uint16_t entryIndex) {
    if (entryIndex >= view.entryCount) {
        return nullptr;
    }
    return view.inf + 16 + static_cast<size_t>(entryIndex) * view.entrySize;
}

static uint16_t bmg_entry_message_id(const MidnaBmgView& view, uint16_t entryIndex) {
    const uint8_t* entry = bmg_entry(view, entryIndex);
    return entry != nullptr ? read_be16(entry + 4) : 0xffff;
}

static uint16_t bmg_entry_for_msg_id(const MidnaBmgView& view, uint16_t msgId) {
    for (uint16_t i = 0; i < view.entryCount; ++i) {
        if (bmg_entry_message_id(view, i) == msgId) {
            return i;
        }
    }
    return mods::flow::kEnd;
}

static const uint8_t* bmg_entry_text(const MidnaBmgView& view, uint16_t entryIndex, size_t& outSize) {
    outSize = 0;
    const uint8_t* entry = bmg_entry(view, entryIndex);
    if (entry == nullptr || view.datSize < 8) {
        return nullptr;
    }

    const size_t offset = read_be32(entry);
    if (offset >= view.datSize - 8) {
        return nullptr;
    }

    const uint8_t* text = view.dat + 8 + offset;
    const size_t maximum = view.datSize - 8 - offset;
    for (size_t i = 0; i < maximum; ++i) {
        if (text[i] == 0) {
            outSize = i + 1;
            return text;
        }
    }
    return nullptr;
}

static const uint8_t* bmg_node(const MidnaBmgView& view, uint16_t nodeIndex) {
    if (nodeIndex >= view.nodeCount) {
        return nullptr;
    }
    return view.nodes + static_cast<size_t>(nodeIndex) * 8;
}

static uint16_t bmg_flow_init_node(const uint8_t* bmg, uint16_t label) {
    const uint8_t* fli = nullptr;
    size_t fliSize = 0;
    if (!find_bmg_section(bmg, MULTI_CHAR('FLI1'), fli, fliSize) || fliSize < 0x10) {
        return mods::flow::kEnd;
    }
    const uint16_t count = read_be16(fli + 8);
    if (static_cast<size_t>(count) * 8 > fliSize - 0x10) {
        return mods::flow::kEnd;
    }
    uint16_t result = mods::flow::kEnd;
    for (uint16_t i = 0; i < count; ++i) {
        const uint8_t* e = fli + 0x10 + static_cast<size_t>(i) * 8;
        if (read_be16(e) == label) {
            result = read_be16(e + 4);
        }
    }
    return result;
}

static uint16_t bmg_edge_target(const MidnaBmgView& view, uint16_t edgeIndex) {
    if (edgeIndex >= view.edgeCount) {
        return mods::flow::kEnd;
    }
    return read_be16(view.edges + static_cast<size_t>(edgeIndex) * 2);
}

static bool valid_native_target(const MidnaBmgView& view, uint16_t target) {
    return target == mods::flow::kEnd || target < view.nodeCount;
}

static uint8_t ascii_lower(uint8_t c) {
    return c >= 'A' && c <= 'Z' ? static_cast<uint8_t>(c + ('a' - 'A')) : c;
}

static bool text_contains_ascii(const uint8_t* text, size_t size, std::string_view needle) {
    if (text == nullptr || needle.empty() || size < needle.size()) {
        return false;
    }
    for (size_t i = 0; i + needle.size() <= size; ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (ascii_lower(text[i + j]) != static_cast<uint8_t>(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

static MidnaTransformOption classify_midna_transform_option(
    const MidnaBmgView& view, uint16_t selectionEntry) {
    size_t textSize = 0;
    const uint8_t* text = bmg_entry_text(view, selectionEntry, textSize);
    if (text == nullptr) {
        return MidnaTransformOption::Unknown;
    }

    constexpr std::array wolfWords{"wolf", "loup", "lobo", "lupo"};
    for (std::string_view word : wolfWords) {
        if (text_contains_ascii(text, textSize, word)) {
            return MidnaTransformOption::Wolf;
        }
    }

    constexpr std::array humanWords{"human", "mensch", "humain", "humano", "umano"};
    for (std::string_view word : humanWords) {
        if (text_contains_ascii(text, textSize, word)) {
            return MidnaTransformOption::Human;
        }
    }
    return MidnaTransformOption::Unknown;
}

static bool is_midna_root_prompt_id(uint16_t messageId) {
    return messageId == kMidnaNoWarpPromptAId || messageId == kMidnaNoWarpPromptBId ||
           messageId == kMidnaMenuPromptId;
}

static bool is_select_query(uint16_t query) {
    return query == FLOW_QUERY_SELECT_2 || query == FLOW_QUERY_SELECT_3 ||
           query == FLOW_QUERY_SELECT_2_CANCEL || query == FLOW_QUERY_SELECT_3_CANCEL;
}

static bool try_make_midna_prompt_patch(const MidnaBmgView& view, uint16_t promptNode,
    MidnaPromptPatchPoint& outPatch) {
    const uint8_t* prompt = bmg_node(view, promptNode);
    if (prompt == nullptr || prompt[0] != 1) {
        return false;
    }

    const uint16_t promptEntry = read_be16(prompt + 2);
    const uint16_t promptMessageId = bmg_entry_message_id(view, promptEntry);
    if (!is_midna_root_prompt_id(promptMessageId)) {
        return false;
    }

    const uint16_t selectionNode = read_be16(prompt + 4);
    const uint8_t* selection = bmg_node(view, selectionNode);
    if (selection == nullptr || selection[0] != 1) {
        return false;
    }

    const uint16_t branchNode = read_be16(selection + 4);
    const uint8_t* branch = bmg_node(view, branchNode);
    if (branch == nullptr || branch[0] != 2 || branch[1] < 2 || !is_select_query(read_be16(branch + 2)))
    {
        return false;
    }

    const uint16_t firstEdge = read_be16(branch + 6);
    if (firstEdge > std::numeric_limits<uint16_t>::max() - branch[1] ||
        static_cast<uint32_t>(firstEdge) + branch[1] > view.edgeCount)
    {
        return false;
    }

    const uint16_t transformTarget = bmg_edge_target(view, firstEdge);
    const uint16_t secondTarget = bmg_edge_target(view, static_cast<uint16_t>(firstEdge + 1));
    const uint16_t thirdTarget = branch[1] >= 3 ?
                                     bmg_edge_target(view, static_cast<uint16_t>(firstEdge + 2)) :
                                     mods::flow::kEnd;
    if (!valid_native_target(view, transformTarget) || !valid_native_target(view, secondTarget) ||
        !valid_native_target(view, thirdTarget))
    {
        return false;
    }

    outPatch = {
        .promptNode = promptNode,
        .promptEntry = promptEntry,
        .promptMessageId = promptMessageId,
        .transformTarget = transformTarget,
        .secondTarget = secondTarget,
        .thirdTarget = thirdTarget,
        .nativeChoiceCount = branch[1],
        .transformOption =
            classify_midna_transform_option(view, read_be16(selection + 2)),
    };
    return outPatch.valid();
}

static void assign_unknown_midna_transform_options(MidnaFlowTopology& topology) {
    size_t noWarpIndex = 0;
    size_t warpIndex = 0;
    for (size_t i = 0; i < topology.promptCount; ++i) {
        auto& patch = topology.prompts[i];
        if (patch.transformOption != MidnaTransformOption::Unknown) {
            continue;
        }

        size_t& index = patch.has_warp_choice() ? warpIndex : noWarpIndex;
        patch.transformOption =
            index == 0 ? MidnaTransformOption::Human : MidnaTransformOption::Wolf;
        ++index;
    }
}

static bool is_boss_rush_link_on_horse() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    return link != nullptr && link->checkHorseRide();
}

static bool is_boss_rush_link_in_water() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) return false;
    if (link->checkModeFlg(0x40000)) return true;
    return link->mWaterY > link->current.pos.y + 20.0f;
}

static bool is_boss_rush_midna_simple_bmg() {
    return is_boss_rush_link_on_horse() || is_boss_rush_link_in_water();
}

static bool discover_midna_flow_topology() {
    dMsgObject_c* msgObject = dMsgObject_getMsgObjectClass();
    if (msgObject == nullptr) {
        return false;
    }

    const auto* bmg = static_cast<const uint8_t*>(msgObject->getMsgDtPtrLocal());
    if (bmg == nullptr || bmg == s_bossRushMidnaTopology.resource) {
        return false;
    }

    static const void* s_lastSeenBmg = nullptr;
    if (bmg != s_lastSeenBmg) {
        boss_rush_debug_log("[midna] bmg=%p simple=%d prompts=%u root3001=%u horse=%p/%u",
                            bmg, (int)is_boss_rush_midna_simple_bmg(),
                            (unsigned)s_bossRushMidnaTopology.promptCount,
                            (unsigned)s_midnaRoot3001, s_bossRushMidnaHorseResource,
                            (unsigned)s_bossRushMidnaHorseNextNode);
        s_lastSeenBmg = bmg;
    }

    if (is_boss_rush_midna_simple_bmg() && s_bossRushMidnaTopology.promptCount != 0 &&
        s_midnaRoot3001 != mods::flow::kEnd) {
        MidnaBmgView view;
        if (parse_midna_bmg_view(bmg, view)) {
            uint16_t node = bmg_flow_init_node(bmg, 0xbb9);
            if (node == mods::flow::kEnd) {
                const uint8_t* triggerNode = bmg_node(view, kMidnaHorseTalkTriggerNode);
                if (triggerNode != nullptr && triggerNode[0] == 1) {
                    node = read_be16(triggerNode + 4);
                }
            }
            if (node != mods::flow::kEnd &&
                (s_bossRushMidnaHorseResource != bmg || s_bossRushMidnaHorseNextNode != node)) {
                s_bossRushMidnaHorseResource = bmg;
                s_bossRushMidnaHorseNextNode = node;
                s_bossRushMidnaTopology.version++;
                boss_rush_debug_log("[midna] horse flow node=%u recorded (bmg=%p)",
                                    (unsigned)node, bmg);
            }
        } else {
            boss_rush_debug_log("[midna] simple bmg=%p parse FAILED", bmg);
        }
        return false;
    }

    MidnaBmgView view;
    if (!parse_midna_bmg_view(bmg, view)) {
        return false;
    }

    MidnaFlowTopology topology;
    topology.resource = bmg;
    for (uint16_t nodeIndex = 0; nodeIndex < view.nodeCount; ++nodeIndex) {
        MidnaPromptPatchPoint patch;
        if (try_make_midna_prompt_patch(view, nodeIndex, patch)) {
            const auto duplicate = std::find_if(topology.prompts.begin(), topology.prompts.end(),
                [&](const MidnaPromptPatchPoint& existing) {
                    return existing.promptNode == patch.promptNode;
                });
            if (duplicate == topology.prompts.end()) {
                topology.prompts.push_back(patch);
                topology.promptCount = topology.prompts.size();
            }
        }
    }

    if (topology.promptCount == 0) {
        if (s_bossRushMidnaHorseResource != bmg) {
            const uint8_t* triggerNode = bmg_node(view, kMidnaHorseTalkTriggerNode);
            if (triggerNode != nullptr && triggerNode[0] == 1) {
                s_bossRushMidnaHorseResource = bmg;
                s_bossRushMidnaHorseNextNode = read_be16(triggerNode + 4);
            }
        }

        return false;
    }

    assign_unknown_midna_transform_options(topology);
    topology.version = s_bossRushMidnaTopology.version + 1;
    s_bossRushMidnaTopology = topology;

    s_midnaRoot3001 = bmg_flow_init_node(bmg, 0xbb9);

    uint16_t v3 = bmg_entry_for_msg_id(view, kMidnaMenuPromptId);
    if (v3 == mods::flow::kEnd) v3 = kMidnaMenuPromptEntry;
    s_midnaVessel3Way = v3;
    uint16_t v2 = bmg_entry_for_msg_id(view, kMidnaNoWarpPromptAId);
    if (v2 == mods::flow::kEnd) v2 = bmg_entry_for_msg_id(view, kMidnaNoWarpPromptBId);
    if (v2 == mods::flow::kEnd) v2 = v3;
    s_midnaVessel2Way = v2;
    return true;
}

static MidnaTransformOption current_midna_transform_option() {
    return dComIfGp_getLinkPlayer() != nullptr && daPy_py_c::checkNowWolf() ?
               MidnaTransformOption::Human :
               MidnaTransformOption::Wolf;
}

static void close_midna_custom_dialog() {
    dMsgObject_onKillMessageFlag();
    daMidna_c* midna = daPy_py_c::getMidnaActor();
    if (midna != nullptr) {
        dComIfGp_getEvent()->reset(midna);
        midna->offStateFlg0(daMidna_c::FLG0_UNK_8000);
    } else {
        dComIfGp_event_reset();
    }
}

void on_boss_rush_leave_midna_event(ModContext*, const FlowEventContext*, void*) {
    if (boss_rush_is_fighting_here()) {
        s_pendingMidnaAction = PendingBossRushMidnaAction::BackToChamber;
    } else {
        s_pendingMidnaAction = PendingBossRushMidnaAction::LeaveChamber;
    }
    s_pendingMidnaDelay = 2;
}

void on_boss_rush_retry_midna_event(ModContext*, const FlowEventContext*, void*) {
    s_pendingMidnaAction = PendingBossRushMidnaAction::RetryFight;
    s_pendingMidnaDelay = 2;
}

static mods::flow::RegisteredMessage register_boss_rush_midna_message_3(
    std::string_view opt1, std::string_view opt2, std::string_view opt3) {
    std::vector<mods::flow::MessageVariant> variants;
    variants.reserve(kAllLanguages.size());
    for (const MessageLanguage language : kAllLanguages) {
        variants.push_back(
            mods::flow::MessageBuilder{}.speaker(kMidnaSpeaker).options(opt1, opt2, opt3).build(language));
    }
    return mods::flow::register_message(kMessageGroup, variants);
}

static mods::flow::RegisteredMessage register_boss_rush_midna_message_2(
    std::string_view opt1, std::string_view opt2) {
    std::vector<mods::flow::MessageVariant> variants;
    variants.reserve(kAllLanguages.size());
    for (const MessageLanguage language : kAllLanguages) {
        variants.push_back(
            mods::flow::MessageBuilder{}.speaker(kMidnaSpeaker).options(opt1, opt2).build(language));
    }
    return mods::flow::register_message(kMessageGroup, variants);
}

bool ensure_boss_rush_midna_messages() {
    if (svc_flow == nullptr) {
        return false;
    }
    if (!s_bossRushMidnaLeaveOnlyMsg) {
        s_bossRushMidnaLeaveOnlyMsg = register_boss_rush_midna_message_2("Leave Boss Rush", "Cancel");
        if (!s_bossRushMidnaLeaveOnlyMsg) {
            return false;
        }
    }
    if (!s_bossRushMidnaBackOnlyMsg) {
        s_bossRushMidnaBackOnlyMsg = register_boss_rush_midna_message_2("Back to Boss Rush", "Cancel");
        if (!s_bossRushMidnaBackOnlyMsg) {
            return false;
        }
    }
    if (!s_bossRushMidnaHumanFightMsg) {
        s_bossRushMidnaHumanFightMsg = register_boss_rush_midna_message_3(
            "Transform to wolf", "Back to Boss Rush", "Cancel");
        if (!s_bossRushMidnaHumanFightMsg) {
            return false;
        }
    }
    if (!s_bossRushMidnaWolfFightMsg) {
        s_bossRushMidnaWolfFightMsg = register_boss_rush_midna_message_3(
            "Transform to human", "Back to Boss Rush", "Cancel");
        if (!s_bossRushMidnaWolfFightMsg) {
            return false;
        }
    }
    return true;
}

class BossRushFlowDraft {
public:
    explicit BossRushFlowDraft(uint16_t group) {
        if (svc_flow == nullptr) {
            mResult = MOD_UNAVAILABLE;
            return;
        }
        mResult = svc_flow->begin_graph(mod_ctx, group, &mHandle);
    }
    BossRushFlowDraft(const BossRushFlowDraft&) = delete;
    BossRushFlowDraft& operator=(const BossRushFlowDraft&) = delete;
    ~BossRushFlowDraft() {
        if (mHandle != 0 && svc_flow != nullptr) {
            svc_flow->remove_graph(mod_ctx, mHandle);
        }
    }

    bool allocate(uint16_t& outId) {
        if (mResult == MOD_OK) {
            mResult = svc_flow->allocate_node(mod_ctx, mHandle, &outId);
        }
        return mResult == MOD_OK;
    }
    bool add_edges(const uint16_t* targets, uint16_t count, uint16_t& outFirst) {
        if (mResult == MOD_OK) {
            mResult = svc_flow->add_edges(mod_ctx, mHandle, targets, count, &outFirst);
        }
        return mResult == MOD_OK;
    }
    bool add_edge(uint16_t target, uint16_t& outEdge) { return add_edges(&target, 1, outEdge); }
    bool fill_node(uint16_t node, const FlowNodeData& data) {
        if (mResult == MOD_OK) {
            mResult = svc_flow->fill_node(mod_ctx, mHandle, node, &data);
        }
        return mResult == MOD_OK;
    }
    bool patch_node(uint16_t node, const FlowNodeData& data) {
        if (mResult == MOD_OK) {
            mResult = svc_flow->patch_node(mod_ctx, mHandle, node, &data);
        }
        return mResult == MOD_OK;
    }
    bool patch_edge(uint16_t edgeIndex, uint16_t targetNode) {
        if (mResult == MOD_OK) {
            mResult = svc_flow->patch_edge(mod_ctx, mHandle, edgeIndex, targetNode);
        }
        return mResult == MOD_OK;
    }
    mods::flow::Graph commit() {
        if (mResult == MOD_OK) {
            mResult = svc_flow->commit_graph(mod_ctx, mHandle);
        }
        if (mResult != MOD_OK) {
            return {0, mResult};
        }
        return {std::exchange(mHandle, 0), MOD_OK};
    }

private:
    FlowGraphHandle mHandle = 0;
    ModResult mResult = MOD_OK;
};

static bool boss_rush_add_message_node(
    BossRushFlowDraft& graph, MessageId messageId, uint16_t target, uint16_t& outNode) {
    return graph.allocate(outNode) &&
           graph.fill_node(outNode, mods::flow::message(0, messageId, target));
}

static bool boss_rush_add_event_node(BossRushFlowDraft& graph, FlowEventId eventId,
                                     std::array<uint8_t, 4> params, uint16_t target, uint16_t& outNode) {
    uint16_t edge = 0;
    return graph.allocate(outNode) && graph.add_edge(target, edge) &&
           graph.fill_node(outNode, mods::flow::event(eventId, edge, params));
}

static bool boss_rush_patch_midna_prompt(BossRushFlowDraft& graph, uint16_t nativePromptNode,
                                         uint16_t selectionNode, uint8_t cancelPosition,
                                         uint16_t vesselEntry) {
    uint16_t promptNode = 0;
    uint16_t setupEdge = 0;
    return boss_rush_add_message_node(graph, vesselEntry, selectionNode, promptNode) &&
           graph.add_edge(promptNode, setupEdge) &&
           graph.patch_node(nativePromptNode,
               mods::flow::event(FLOW_EVENT_SELECT_VERTICAL, setupEdge, {0, 0, 0, cancelPosition}));
}

static bool boss_rush_build_three_choice_prompt(BossRushFlowDraft& graph, uint16_t nativePromptNode,
    MessageId messageId, uint16_t choice0Target, uint16_t choice1Target) {
    uint16_t choice = 0;
    uint16_t selection = 0;
    uint16_t firstEdge = 0;
    const std::array<uint16_t, 4> targets{
        choice0Target, choice1Target, mods::flow::kEnd, mods::flow::kEnd};

    return graph.allocate(choice) &&
           graph.add_edges(targets.data(), static_cast<uint16_t>(targets.size()), firstEdge) &&
           graph.fill_node(choice,
               mods::flow::branch(
                   static_cast<uint8_t>(targets.size()), FLOW_QUERY_SELECT_3_CANCEL, 0, firstEdge)) &&
           boss_rush_add_message_node(graph, messageId, choice, selection) &&
           boss_rush_patch_midna_prompt(graph, nativePromptNode, selection, 4, s_midnaVessel3Way);
}

static bool boss_rush_build_triple_option_prompt(BossRushFlowDraft& graph, uint16_t nativePromptNode,
    MessageId messageId, uint16_t t0, uint16_t t1, uint16_t t2) {
    uint16_t choice = 0;
    uint16_t selection = 0;
    uint16_t firstEdge = 0;
    const std::array<uint16_t, 4> targets{t0, t1, t2, mods::flow::kEnd};

    return graph.allocate(choice) &&
           graph.add_edges(targets.data(), static_cast<uint16_t>(targets.size()), firstEdge) &&
           graph.fill_node(choice,
               mods::flow::branch(
                   static_cast<uint8_t>(targets.size()), FLOW_QUERY_SELECT_3_CANCEL, 0, firstEdge)) &&
           boss_rush_add_message_node(graph, messageId, choice, selection) &&
           boss_rush_patch_midna_prompt(graph, nativePromptNode, selection, 4, s_midnaVessel3Way);
}

static bool boss_rush_build_two_choice_prompt(BossRushFlowDraft& graph, uint16_t nativePromptNode,
    MessageId messageId, uint16_t choice0Target) {
    uint16_t choice = 0;
    uint16_t selection = 0;
    uint16_t firstEdge = 0;
    const std::array<uint16_t, 3> targets{
        choice0Target, mods::flow::kEnd, mods::flow::kEnd};

    return graph.allocate(choice) &&
           graph.add_edges(targets.data(), static_cast<uint16_t>(targets.size()), firstEdge) &&
           graph.fill_node(choice,
               mods::flow::branch(
                   static_cast<uint8_t>(targets.size()), FLOW_QUERY_SELECT_2_CANCEL, 0, firstEdge)) &&
           boss_rush_add_message_node(graph, messageId, choice, selection) &&
           boss_rush_patch_midna_prompt(graph, nativePromptNode, selection, 3, s_midnaVessel2Way);
}

static mods::flow::Graph build_boss_rush_midna_graph(BossRushMidnaMode mode) {
    BossRushFlowDraft graph{kMessageGroup};

    uint16_t leaveEvent = 0;
    if (!boss_rush_add_event_node(
            graph, s_bossRushLeaveMidnaEvent.id(), {0, 0, 0, 0}, mods::flow::kEnd, leaveEvent))
    {
        return graph.commit();
    }

    uint16_t retryEvent = mods::flow::kEnd;
    if (mode == BossRushMidnaMode::Fight && s_bossRushRetryMidnaEvent &&
        !boss_rush_add_event_node(
            graph, s_bossRushRetryMidnaEvent.id(), {0, 0, 0, 0}, mods::flow::kEnd, retryEvent))
    {
        return graph.commit();
    }

    if (mode == BossRushMidnaMode::Chamber && s_midnaRoot3001 != mods::flow::kEnd) {
        if (boss_rush_build_two_choice_prompt(graph, s_midnaRoot3001,
                s_bossRushMidnaLeaveOnlyMsg.id(), leaveEvent))
        {
            return graph.commit();
        }
    }
    if (mode == BossRushMidnaMode::Chamber && s_bossRushMidnaTopology.promptCount != 0) {
        for (size_t i = 0; i < s_bossRushMidnaTopology.promptCount; ++i) {
            if (!boss_rush_build_two_choice_prompt(graph, s_bossRushMidnaTopology.prompts[i].promptNode,
                    s_bossRushMidnaLeaveOnlyMsg.id(), leaveEvent))
            {
                return graph.commit();
            }
        }
        return graph.commit();
    }

    const bool wantSimpleBmgMenu = mode == BossRushMidnaMode::Fight &&
        (is_boss_rush_link_in_water() || is_boss_rush_link_on_horse());

    const char* targetName = boss_rush_current_target_name();
    const char* curStage = dComIfGp_getStartStageName();
    bool isBeastGanonPhase = boss_rush_gauntlet_phase() == 3;
    if (!isBeastGanonPhase && targetName != nullptr && std::strcmp(targetName, "Beast Ganon") == 0) {
        isBeastGanonPhase = true;
    } else if (curStage != nullptr && std::strcmp(curStage, "D_MN09A") == 0) {
        if (dComIfG_play_c::getLayerNo(0) == 1 || fopAcM_SearchByName(fpcNm_B_MGN_e) != nullptr) {
            isBeastGanonPhase = true;
        }
    }
    const bool isDeathSword = targetName != nullptr && std::strcmp(targetName, "Death Sword") == 0;
    const bool transformAllowed = isDeathSword || isBeastGanonPhase;

    if (mode == BossRushMidnaMode::Fight && (wantSimpleBmgMenu || !transformAllowed)) {
        if (wantSimpleBmgMenu && s_bossRushMidnaHorseNextNode != mods::flow::kEnd) {
            const bool ok = boss_rush_build_two_choice_prompt(
                graph, s_bossRushMidnaHorseNextNode,
                s_bossRushMidnaBackOnlyMsg.id(), leaveEvent);
            boss_rush_debug_log("[midna] horse graph node=%u built=%d",
                                (unsigned)s_bossRushMidnaHorseNextNode, (int)ok);
            return graph.commit();
        }
        if (s_midnaRoot3001 != mods::flow::kEnd) {
            boss_rush_build_two_choice_prompt(graph, s_midnaRoot3001,
                    s_bossRushMidnaBackOnlyMsg.id(), leaveEvent);
        }
        if (s_bossRushMidnaTopology.promptCount != 0) {
            for (size_t i = 0; i < s_bossRushMidnaTopology.promptCount; ++i) {
                if (!boss_rush_build_two_choice_prompt(graph, s_bossRushMidnaTopology.prompts[i].promptNode,
                        s_bossRushMidnaBackOnlyMsg.id(), leaveEvent))
                {
                    return graph.commit();
                }
            }
            return graph.commit();
        }
        if (s_midnaRoot3001 != mods::flow::kEnd) {
            return graph.commit();
        }
        if (!boss_rush_build_two_choice_prompt(
                graph, kMidnaPromptHumanNode, s_bossRushMidnaBackOnlyMsg.id(), leaveEvent) ||
            !boss_rush_build_two_choice_prompt(
                graph, kMidnaPromptWolfNode, s_bossRushMidnaBackOnlyMsg.id(), leaveEvent))
        {
            return graph.commit();
        }
        return graph.commit();
    }

    const MessageId currentFormMsgId = (current_midna_transform_option() == MidnaTransformOption::Human) ?
        s_bossRushMidnaWolfFightMsg.id() : s_bossRushMidnaHumanFightMsg.id();

    if (s_bossRushMidnaTopology.promptCount != 0) {
        for (size_t i = 0; i < s_bossRushMidnaTopology.promptCount; ++i) {
            const MidnaPromptPatchPoint& patch = s_bossRushMidnaTopology.prompts[i];
            MessageId patchMsgId = currentFormMsgId;

            if (!boss_rush_build_three_choice_prompt(
                    graph, patch.promptNode, patchMsgId, patch.transformTarget, leaveEvent))
            {
                return graph.commit();
            }
        }
    } else {
        if (!boss_rush_build_three_choice_prompt(
                graph, kMidnaPromptHumanNode, s_bossRushMidnaHumanFightMsg.id(),
                kMidnaHumanBranch, leaveEvent) ||
            !boss_rush_build_three_choice_prompt(
                graph, kMidnaPromptWolfNode, s_bossRushMidnaWolfFightMsg.id(),
                kMidnaWolfBranch, leaveEvent))
        {
            return graph.commit();
        }
    }

    return graph.commit();
}

static HookAction on_not_talk_pre(ModContext*, void*, void* ret, void*) {
    if (!is_boss_rush_active() || is_boss_rush_transition_in_flight()) {
        return HOOK_CONTINUE;
    }
    *static_cast<BOOL*>(ret) = FALSE;
    return HOOK_SKIP_ORIGINAL;
}

static HookAction on_order_z_talk_pre(ModContext*, void* args, void* ret, void*) {
    auto* link = mods::arg<daAlink_c*>(args, 0);
    if (!link) {
        return HOOK_CONTINUE;
    }

    if (!is_boss_rush_active() || is_boss_rush_transition_in_flight()) {
        return HOOK_CONTINUE;
    }

    dMeter2Info_onUseButton(METER2_USEBUTTON_Z);

    bool triggered = link->midnaTalkTrigger() || g_dpadLeftTrig;
    // The z-button mod captures dpad-left only while no menu/event state is
    // up; while those states are active the raw button stays untouched in the
    // pad info, so read it here to keep the call working in every state.
    if (g_configCustomZButtonEnabled) {
        triggered = triggered ||
            (mDoCPd_c::getCpadInfo(PAD_1).mPressedButtonFlags & PAD_BUTTON_LEFT) != 0;
    }
#if PLATFORM_GCN
    triggered = triggered || mDoCPd_c::getTrigZ(PAD_1);
#endif

    if (triggered) {
        s_talkHoldTicks = 600;

        fopAc_ac_c* midna = daAlink_c::getMidnaActor();

        if (midna == nullptr) {
            midna = fopAcM_SearchByName(fpcNm_MIDNA_e);
        }

        bool usedLinkAsMidna = false;
        if (midna == nullptr) {
            daPy_py_c::setMidnaActor(link);
            midna = link;
            usedLinkAsMidna = true;
        }

        if (link->mMidnaMsg != nullptr) {
            dComIfGp_setMesgCameraInfoActor(link->mMidnaMsg, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
        s32 res = fopAcM_orderTalkEvent(link, midna, 0x1FF, 0x400);
        if (res == 0) {
            fopAcM_orderTalkEvent(link, midna, 0, 0);
        }
        link->field_0x35a0 = link->field_0x3594;

        if (usedLinkAsMidna) {
            daPy_py_c::setMidnaActor(nullptr);
        }

        *static_cast<int*>(ret) = 1;
        return HOOK_SKIP_ORIGINAL;
    }

    *static_cast<int*>(ret) = 0;
    return HOOK_SKIP_ORIGINAL;
}

}

bool is_boss_rush_ganon_stage(const char* stage) {
    if (stage == nullptr) return false;
    return std::strcmp(stage, "D_MN09A") == 0 ||
           std::strcmp(stage, "D_MN09B") == 0 ||
           std::strcmp(stage, "D_MN09C") == 0;
}

bool is_boss_rush_ganon_fight() {
    return is_boss_rush_active() && !is_in_boss_rush_chamber() && boss_rush_is_fighting_here() && !is_boss_rush_transition_in_flight();
}

bool boss_rush_midna_talk_hold_active() {
    return s_talkHoldTicks > 0;
}

void update_boss_rush_midna(const LogService*, ModContext*) {
    if (s_talkHoldTicks > 0) {
        --s_talkHoldTicks;
    }

    if (!is_boss_rush_active() || is_boss_rush_transition_in_flight()) {
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == nullptr) {
        return;
    }

    if (is_in_boss_rush_chamber()) {
        // A fresh boss-rush save keeps the Midna availability bits unset
        // (M_067 riding bit, 0x0540 back-ride bit) with F_0800 "Midna can't
        // be called" set. The engine then fades the HUD Midna icon out while
        // the z-button HUD keeps re-showing it, which flickers per frame.
        // Hold the bits while waiting in the chamber, like the Ganon fight
        // workaround does.
        dComIfGs_offEventBit(dSv_event_flag_c::F_0800);
        dComIfGs_onEventBit(dSv_event_flag_c::M_067);
        dComIfGs_onEventBit(0x0540);
    }

    dMeter2Info_onUseButton(METER2_USEBUTTON_Z);
}


ModResult init_boss_rush_midna(const HookService* hook_svc, const LogService*, ModContext*) {
    shutdown_boss_rush_midna();

    if (hook_svc != nullptr) {
        mods::hook::add_pre<BossRushOrderZTalkHook>(hook_svc, on_order_z_talk_pre);
        mods::hook::add_pre<BossRushNotTalkHook>(hook_svc, on_not_talk_pre);
    }

    if (svc_flow != nullptr) {
        s_bossRushLeaveMidnaEvent =
            mods::flow::register_event("BossRushLeaveMidnaMenu", on_boss_rush_leave_midna_event);
        s_bossRushRetryMidnaEvent =
            mods::flow::register_event("BossRushRetryMidnaMenu", on_boss_rush_retry_midna_event);
    }

    return MOD_OK;
}

void reset_boss_rush_midna_flow() {
    s_bossRushMidnaFlowGraph.reset();
    s_bossRushMidnaMode = BossRushMidnaMode::None;
    s_pendingMidnaAction = PendingBossRushMidnaAction::None;
    s_pendingMidnaDelay = 0;
}

void shutdown_boss_rush_midna() {
    reset_boss_rush_midna_flow();
    s_bossRushMidnaTopologyVersion = 0;
    s_bossRushMidnaTransformOption = MidnaTransformOption::Unknown;
    s_bossRushMidnaTopology = {};
    s_bossRushMidnaHorseback = false;
    s_bossRushMidnaHorseResource = nullptr;
    s_bossRushMidnaHorseNextNode = mods::flow::kEnd;

    s_bossRushMidnaLeaveOnlyMsg.reset();
    s_bossRushMidnaBackOnlyMsg.reset();
    s_bossRushMidnaFightBackMsg.reset();
    s_bossRushMidnaHumanFightMsg.reset();
    s_bossRushMidnaWolfFightMsg.reset();
}

bool process_pending_boss_rush_midna_action(const LogService* log_svc, ModContext* mod_ctx) {
    if (s_pendingMidnaAction == PendingBossRushMidnaAction::None) {
        return false;
    }

    refresh_boss_rush_midna_flow();
    if (s_pendingMidnaDelay > 0) {
        --s_pendingMidnaDelay;
        return true;
    }

    const PendingBossRushMidnaAction action = s_pendingMidnaAction;
    s_pendingMidnaAction = PendingBossRushMidnaAction::None;
    close_midna_custom_dialog();

    if (action == PendingBossRushMidnaAction::BackToChamber) {
        return_to_boss_rush_chamber(log_svc, mod_ctx, "Left via Midna's call menu");
    } else if (action == PendingBossRushMidnaAction::LeaveChamber) {
        exit_boss_rush();
    } else if (action == PendingBossRushMidnaAction::RetryFight) {
        boss_rush_retry_current_fight(log_svc, mod_ctx);
    }
    return true;
}

void refresh_boss_rush_midna_flow() {
    discover_midna_flow_topology();

    const bool pendingAction = s_pendingMidnaAction != PendingBossRushMidnaAction::None;
    const bool enableNextStage = dComIfGp_isEnableNextStage();
    const bool peek = fopOvlpM_IsPeek();

    if (pendingAction) {
        if (s_bossRushMidnaMode != BossRushMidnaMode::None) {
            s_bossRushMidnaFlowGraph.reset();
            s_bossRushMidnaMode = BossRushMidnaMode::None;
        }
        return;
    }

    if ((enableNextStage || peek) && s_bossRushMidnaMode == BossRushMidnaMode::None) {
        return;
    }

    BossRushMidnaMode wantMode = BossRushMidnaMode::None;
    if (is_boss_rush_active()) {
        if (boss_rush_is_fighting_here()) {
            wantMode = BossRushMidnaMode::Fight;
        } else if (is_in_boss_rush_chamber()) {
            wantMode = BossRushMidnaMode::Chamber;
        }
    }

    static BossRushMidnaMode s_lastWantMode = BossRushMidnaMode::None;
    if (wantMode != s_lastWantMode) {
        boss_rush_debug_log("[midna] wantMode %d -> %d (fightingHere=%d chamber=%d)",
                            (int)s_lastWantMode, (int)wantMode,
                            (int)boss_rush_is_fighting_here(), (int)is_in_boss_rush_chamber());
        s_lastWantMode = wantMode;
    }

    const MidnaTransformOption transformOption = (wantMode != BossRushMidnaMode::None) ?
        current_midna_transform_option() : MidnaTransformOption::Unknown;
    const int gauntletPhase = (wantMode == BossRushMidnaMode::Fight) ?
        boss_rush_gauntlet_phase() : 0;
    const bool horseback = wantMode == BossRushMidnaMode::Fight && is_boss_rush_midna_simple_bmg();

    if (wantMode == s_bossRushMidnaMode &&
        s_bossRushMidnaFlowGraph.handle() != 0 &&
        s_bossRushMidnaTopologyVersion == s_bossRushMidnaTopology.version &&
        s_bossRushMidnaTransformOption == transformOption &&
        s_bossRushMidnaGauntletPhase == gauntletPhase &&
        s_bossRushMidnaHorseback == horseback) {
        return;
    }

    s_bossRushMidnaFlowGraph.reset();
    s_bossRushMidnaMode = BossRushMidnaMode::None;

    if (wantMode == BossRushMidnaMode::None) {
        return;
    }
    if (svc_flow == nullptr) {
        return;
    }
    if (!s_bossRushLeaveMidnaEvent) {
        return;
    }
    if (!ensure_boss_rush_midna_messages()) {
        return;
    }

    s_bossRushMidnaFlowGraph = build_boss_rush_midna_graph(wantMode);
    if (s_bossRushMidnaFlowGraph) {
        s_bossRushMidnaMode = wantMode;
        s_bossRushMidnaTopologyVersion = s_bossRushMidnaTopology.version;
        s_bossRushMidnaTransformOption = transformOption;
        s_bossRushMidnaHorseback = horseback;
        s_bossRushMidnaGauntletPhase = gauntletPhase;
        boss_rush_debug_log("[midna] graph built mode=%d horse=%d horseNode=%u ver=%u "
                            "prompts=%u root3001=%u gphase=%d",
                            (int)wantMode, (int)horseback,
                            (unsigned)s_bossRushMidnaHorseNextNode,
                            (unsigned)s_bossRushMidnaTopology.version,
                            (unsigned)s_bossRushMidnaTopology.promptCount,
                            (unsigned)s_midnaRoot3001, gauntletPhase);
    } else {
        boss_rush_debug_log("[midna] graph build FAILED mode=%d", (int)wantMode);
    }
}

#include "boss_rush_hardmode.hpp"
#include "boss_rush.hpp"
#include "boss_rush_common.hpp"
#include "boss_rush_darklink.hpp"
#include "../actor_attribute.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_camera_mng.h"
#include "d/actor/d_a_b_gnd.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_lib.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/JUtility/TColor.h"

#include <dolphin/gx.h>
#include <dolphin/gx/GXVert.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

struct HardModeBoss {
    const char* name;
    f32 health;
    f32 damage;
    f32 speed;
    f32 size;
    f32 notice;
    f32 knockback;
    f32 stun;
};

constexpr HardModeBoss kHardModeBosses[] = {
    {"Ook", 1.5f, 1.5f, 1.3f, 1.0f, 1.5f, 1.25f, 0.75f},
    {"Diababa", 1.5f, 2.0f, 1.25f, 1.0f, 1.0f, 1.25f, 0.7f},
    {"Dangoro", 1.5f, 1.5f, 1.25f, 1.1f, 1.0f, 1.75f, 0.75f},
    {"Fyrus", 1.5f, 1.75f, 1.2f, 1.15f, 1.0f, 1.25f, 0.75f},
    {"Deku Toad", 1.5f, 1.5f, 1.3f, 1.25f, 1.5f, 1.5f, 0.75f},
    {"Morpheel", 1.4f, 1.75f, 1.2f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Death Sword", 1.5f, 1.5f, 1.25f, 1.0f, 1.5f, 1.25f, 0.7f},
    {"Stallord", 1.4f, 1.5f, 1.2f, 1.0f, 1.0f, 1.25f, 1.0f},
    {"Darkhammer", 1.5f, 1.5f, 1.2f, 1.0f, 1.25f, 1.5f, 0.75f},
    {"Blizzeta", 1.4f, 1.5f, 1.25f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Darknut", 1.5f, 2.0f, 1.3f, 1.0f, 1.5f, 1.25f, 0.6f},
    {"Armogohma", 1.4f, 1.5f, 1.3f, 1.0f, 1.0f, 1.0f, 0.7f},
    {"Aeralfos", 1.5f, 1.75f, 1.3f, 1.0f, 1.25f, 1.5f, 0.7f},
    {"Argorok", 1.4f, 1.75f, 1.15f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Zant", 1.4f, 1.75f, 1.25f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Puppet Zelda", 1.67f, 2.0f, 1.25f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Beast Ganon", 1.4f, 1.75f, 1.2f, 1.15f, 1.0f, 1.5f, 0.7f},
    {"Horseback Ganon", 1.25f, 1.5f, 1.15f, 1.0f, 1.0f, 1.25f, 0.75f},
    {"Ganondorf", 1.5f, 2.0f, 1.25f, 1.0f, 1.0f, 1.25f, 0.7f},
    {"Dark Link", 1.5f, 1.5f, 1.2f, 1.0f, 1.0f, 1.25f, 0.75f},
};

constexpr f32 kTintFadeStep = 1.0f / 30.0f;
constexpr s16 kActorTint[3] = {5, -2, -2};
constexpr s16 kBgTint[3] = {6, -2, -2};
constexpr s16 kFogTint[3] = {9, -3, -3};
constexpr s16 kSkyTint[3] = {8, -3, -3};

constexpr int kEmberCount = 110;
constexpr f32 kEmberSpawnRadius = 1100.0f;
constexpr f32 kEmberRespawnRadius = 1400.0f;
constexpr f32 kEmberMinHeight = 600.0f;
constexpr f32 kEmberHeightRange = 700.0f;
constexpr f32 kEmberFloorFade = 150.0f;
constexpr int kEmberFadeInTicks = 20;

constexpr f32 kEmberTailTicks = 7.0f;
constexpr int kGlowSegs = 14;
constexpr int kCoreSegs = 10;
constexpr int kAshSegs = 7;

struct Ember {
    cXyz pos;
    cXyz vel;
    f32 fallSpeed;
    f32 phase;
    f32 size;
    f32 spin;
    f32 heat;
    int age;
    bool ash;
    bool alive;
};

Ember s_embers[kEmberCount] = {};
u32 s_emberSeed = 0x2545F491u;
u32 s_emberTick = 0;

const LogService* s_logSvc = nullptr;
ModContext* s_modCtx = nullptr;
const ConfigService* s_configSvc = nullptr;
ConfigVarHandle s_configVar = 0;
ActorAttributeResolverHandle s_resolverHandle = 0;
bool s_enabled = false;
f32 s_tint = 0.0f;
bool s_tintApplied = false;
s16 s_darkLinkProfile = -1;

void log_info(const char* msg) {
    if (s_logSvc != nullptr && s_modCtx != nullptr) {
        s_logSvc->info(s_modCtx, msg);
    }
}

const HardModeBoss* find_boss(const char* name) {
    if (name == nullptr) {
        return nullptr;
    }
    for (const HardModeBoss& boss : kHardModeBosses) {
        if (std::strcmp(boss.name, name) == 0) {
            return &boss;
        }
    }
    return nullptr;
}

bool ganondorf_on_horse(const fopAc_ac_c* actor) {
    const auto* gnd = reinterpret_cast<const b_gnd_class*>(actor);
    if (gnd->mDrawHorse != 0) {
        return true;
    }
    const char* target = boss_rush_current_target_name();
    return target != nullptr && std::strcmp(target, "Horseback Ganon") == 0 &&
           gnd->mActionMode >= 1 && gnd->mActionMode <= 6;
}

const char* boss_name_for_actor(const fopAc_ac_c* actor) {
    auto* mutableActor = const_cast<fopAc_ac_c*>(actor);
    const s16 profile = fopAcM_GetName(mutableActor);
    if (s_darkLinkProfile >= 0 && profile == s_darkLinkProfile) {
        return "Dark Link";
    }
    switch (profile) {
    case fpcNm_E_MK_e:
    case fpcNm_E_MK_BO_e:
        return "Ook";
    case fpcNm_B_BQ_e:
    case fpcNm_B_BH_e:
    case fpcNm_E_MB_e:
        return "Diababa";
    case fpcNm_E_GOB_e:
        return "Dangoro";
    case fpcNm_E_FM_e:
        return "Fyrus";
    case fpcNm_E_DT_e:
        return "Deku Toad";
    case fpcNm_B_OB_e:
    case fpcNm_B_OH_e:
    case fpcNm_B_OH2_e:
        return "Morpheel";
    case fpcNm_E_VT_e:
        return "Death Sword";
    case fpcNm_B_DS_e:
        return "Stallord";
    case fpcNm_E_TH_e:
    case fpcNm_E_TH_BALL_e:
        return "Darkhammer";
    case fpcNm_B_YO_e:
    case fpcNm_B_YOI_e:
        return "Blizzeta";
    case fpcNm_B_TN_e:
        return boss_rush_darklink_replaces_darknut(actor) ? "Dark Link" : "Darknut";
    case fpcNm_B_GM_e:
        return "Armogohma";
    case fpcNm_B_GG_e:
        return "Aeralfos";
    case fpcNm_B_DR_e:
    case fpcNm_B_DRE_e:
        return "Argorok";
    case fpcNm_B_ZANT_e:
    case fpcNm_B_ZANTM_e:
    case fpcNm_B_ZANTZ_e:
    case fpcNm_B_ZANTS_e:
        return "Zant";
    case fpcNm_E_HZELDA_e:
        return "Puppet Zelda";
    case fpcNm_B_MGN_e:
        return "Beast Ganon";
    case fpcNm_B_GND_e:
        return ganondorf_on_horse(actor) ? "Horseback Ganon" : "Ganondorf";
    default:
        return nullptr;
    }
}

const HardModeBoss* boss_for_actor(const fopAc_ac_c* actor) {
    if (actor == nullptr || !boss_rush_hardmode_active()) {
        return nullptr;
    }
    return find_boss(boss_name_for_actor(actor));
}

f32 ember_rand() {
    s_emberSeed = s_emberSeed * 1664525u + 1013904223u;
    return static_cast<f32>(s_emberSeed >> 8) * (1.0f / 16777216.0f);
}

bool ember_camera(cXyz& eye, cXyz& center) {
    camera_process_class* cam = dComIfGp_getCamera(0);
    if (cam == nullptr) {
        return false;
    }
    eye.set(cam->view.lookat.eye.x, cam->view.lookat.eye.y, cam->view.lookat.eye.z);
    center.set(cam->view.lookat.center.x, cam->view.lookat.center.y, cam->view.lookat.center.z);
    return true;
}

void spawn_ember(Ember& e, const cXyz& center, bool anywhere) {
    const f32 ang = ember_rand() * 6.2831853f;
    const f32 rad = std::sqrt(ember_rand()) * kEmberSpawnRadius;
    const f32 top = kBossChamberFloorY + kEmberMinHeight + ember_rand() * kEmberHeightRange;
    e.pos.x = center.x + std::sin(ang) * rad;
    e.pos.z = center.z + std::cos(ang) * rad;
    e.pos.y = anywhere ? kBossChamberFloorY + ember_rand() * (top - kBossChamberFloorY) : top;
    e.ash = ember_rand() < 0.35f;
    e.fallSpeed = e.ash ? 1.6f + ember_rand() * 1.8f : 2.8f + ember_rand() * 3.4f;
    e.phase = ember_rand() * 6.2831853f;
    e.size = e.ash ? 0.8f + ember_rand() * 0.9f : 0.6f + ember_rand() * 0.7f;
    e.spin = (ember_rand() - 0.5f) * 0.12f;
    e.heat = 0.55f + ember_rand() * 0.45f;
    e.vel.set(0.0f, -e.fallSpeed, 0.0f);
    e.age = 0;
    e.alive = true;
}

void update_embers(bool active) {
    if (!active) {
        for (Ember& e : s_embers) {
            e.alive = false;
        }
        return;
    }
    cXyz eye;
    cXyz center;
    if (!ember_camera(eye, center)) {
        return;
    }
    ++s_emberTick;
    const f32 t = static_cast<f32>(s_emberTick);
    for (Ember& e : s_embers) {
        if (!e.alive) {
            spawn_ember(e, center, true);
            continue;
        }
        const f32 dx = e.pos.x - center.x;
        const f32 dz = e.pos.z - center.z;
        if (e.pos.y < kBossChamberFloorY || dx * dx + dz * dz > kEmberRespawnRadius * kEmberRespawnRadius) {
            spawn_ember(e, center, false);
            continue;
        }
        const f32 sway = e.ash ? 0.9f : 0.5f;
        e.vel.x = std::sin(t * 0.045f + e.phase) * sway + 0.25f;
        e.vel.y = -e.fallSpeed;
        e.vel.z = std::cos(t * 0.038f + e.phase * 1.3f) * sway;
        e.pos += e.vel;
        if (e.age < kEmberFadeInTicks) {
            ++e.age;
        }
    }
}

bool project_ember(const Ember& e, const cXyz& eye, Vec& screen, f32& size, f32& alpha) {
    if (!e.alive) {
        return false;
    }
    alpha = s_tint * static_cast<f32>(e.age) / static_cast<f32>(kEmberFadeInTicks);
    const f32 floorFade = (e.pos.y - kBossChamberFloorY) / kEmberFloorFade;
    alpha *= floorFade < 0.0f ? 0.0f : (floorFade > 1.0f ? 1.0f : floorFade);
    if (alpha <= 0.01f) {
        return false;
    }
    const f32 dist = (e.pos - eye).abs();
    if (dist < 80.0f) {
        return false;
    }
    cXyz pos = e.pos;
    mDoLib_project(&pos, &screen);
    if (screen.z >= 400000.0f || screen.x < -40.0f || screen.x > 680.0f ||
        screen.y < -40.0f || screen.y > 520.0f) {
        return false;
    }
    size = 2400.0f / dist * e.size;
    size = size < 1.0f ? 1.0f : (size > 5.5f ? 5.5f : size);
    return true;
}

void draw_fan(f32 cx, f32 cy, f32 rx, f32 ry, f32 rot, GXColor inner, GXColor outer, int segs) {
    const f32 cr = std::cos(rot);
    const f32 sr = std::sin(rot);
    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, segs + 2);
    GXPosition3f32(cx, cy, 0.0f);
    GXColor4u8(inner.r, inner.g, inner.b, inner.a);
    for (int i = 0; i <= segs; ++i) {
        const f32 a = static_cast<f32>(i) * (6.2831853f / static_cast<f32>(segs));
        const f32 lx = std::cos(a) * rx;
        const f32 ly = std::sin(a) * ry;
        GXPosition3f32(cx + lx * cr - ly * sr, cy + lx * sr + ly * cr, 0.0f);
        GXColor4u8(outer.r, outer.g, outer.b, outer.a);
    }
    GXEnd();
}

void draw_streak(f32 hx, f32 hy, f32 tx, f32 ty, f32 width, GXColor head, GXColor tail) {
    f32 dx = tx - hx;
    f32 dy = ty - hy;
    const f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.5f) {
        return;
    }
    dx /= len;
    dy /= len;
    const f32 px = -dy * width * 0.5f;
    const f32 py = dx * width * 0.5f;
    GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
    GXPosition3f32(hx + px, hy + py, 0.0f);
    GXColor4u8(head.r, head.g, head.b, head.a);
    GXPosition3f32(hx - px, hy - py, 0.0f);
    GXColor4u8(head.r, head.g, head.b, head.a);
    GXPosition3f32(tx, ty, 0.0f);
    GXColor4u8(tail.r, tail.g, tail.b, tail.a);
    GXEnd();
}

f32 multiplier_for(const HardModeBoss& boss, ActorAttribute attribute) {
    switch (attribute) {
    case ACTOR_ATTRIBUTE_MOVEMENT_SPEED:
        return boss.speed;
    case ACTOR_ATTRIBUTE_SIZE:
        return boss.size;
    case ACTOR_ATTRIBUTE_HEALTH:
        return boss.health;
    case ACTOR_ATTRIBUTE_ATTACK_DAMAGE:
        return boss.damage;
    case ACTOR_ATTRIBUTE_GRAVITY:
        return boss.speed * boss.speed;
    case ACTOR_ATTRIBUTE_NOTICE_RANGE:
        return boss.notice;
    case ACTOR_ATTRIBUTE_PLAYER_KNOCKBACK:
        return boss.knockback;
    case ACTOR_ATTRIBUTE_STUN_DURATION:
        return boss.stun;
    default:
        return 1.0f;
    }
}

bool resolve_hard_mode_attribute(ModContext*, const ActorAttributeInfo* info, float* outValue, void*) {
    if (info == nullptr || outValue == nullptr || info->actor == nullptr) {
        return false;
    }
    const HardModeBoss* boss = boss_for_actor(static_cast<const fopAc_ac_c*>(info->actor));
    if (boss == nullptr) {
        return false;
    }
    const f32 mult = multiplier_for(*boss, info->attribute);
    if (mult == 1.0f) {
        return false;
    }
    *outValue = info->current_value * mult;
    return true;
}

void set_tint(GXColorS10& color, const s16 (&tint)[3], f32 t) {
    color.r = static_cast<s16>(tint[0] * t);
    color.g = static_cast<s16>(tint[1] * t);
    color.b = static_cast<s16>(tint[2] * t);
}

void apply_tint(f32 t) {
    dScnKy_env_light_c& env = g_env_light;
    set_tint(env.actor_addcol_amb, kActorTint, t);
    set_tint(env.bg_addcol_amb, kBgTint, t);
    set_tint(env.bg1_addcol_amb, kBgTint, t);
    set_tint(env.bg2_addcol_amb, kBgTint, t);
    set_tint(env.bg3_addcol_amb, kBgTint, t);
    set_tint(env.addcol_fog, kFogTint, t);
    set_tint(env.vrbox_addcol_sky0, kSkyTint, t);
    set_tint(env.vrbox_addcol_kasumi, kSkyTint, t);
}

}

void init_boss_rush_hardmode(const LogService* log_svc, ModContext* mod_ctx) {
    s_logSvc = log_svc;
    s_modCtx = mod_ctx;
    s_tint = 0.0f;
    s_tintApplied = false;

    if (svc_actor_attribute == nullptr) {
        log_info("[HardMode] Actor Attribute service missing - hard mode only tints the lighting");
        return;
    }
    if (s_resolverHandle != 0) {
        return;
    }
    const ModResult result = svc_actor_attribute->register_resolver(
        mod_ctx, resolve_hard_mode_attribute, nullptr, &s_resolverHandle);
    if (result != MOD_OK) {
        s_resolverHandle = 0;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "[HardMode] register_resolver failed (%d)", static_cast<int>(result));
        log_info(buf);
        return;
    }
    log_info("[HardMode] attribute resolver registered");
}

void shutdown_boss_rush_hardmode() {
    if (s_resolverHandle != 0 && svc_actor_attribute != nullptr && s_modCtx != nullptr) {
        svc_actor_attribute->unregister_resolver(s_modCtx, s_resolverHandle);
    }
    s_resolverHandle = 0;
    if (s_tintApplied) {
        apply_tint(0.0f);
    }
    s_tint = 0.0f;
    s_tintApplied = false;
    s_darkLinkProfile = -1;
    update_embers(false);
}

void update_boss_rush_hardmode() {
    s_darkLinkProfile = boss_rush_hardmode_active() ? boss_rush_darklink_actor_profile() : -1;

    const bool chamber = boss_rush_hardmode_active() && is_in_boss_rush_chamber();
    const f32 target = chamber ? 1.0f : 0.0f;
    if (s_tint < target) {
        s_tint = s_tint + kTintFadeStep > target ? target : s_tint + kTintFadeStep;
    } else if (s_tint > target) {
        s_tint = s_tint - kTintFadeStep < target ? target : s_tint - kTintFadeStep;
    }

    if (s_tint > 0.0f) {
        apply_tint(s_tint);
        s_tintApplied = true;
    } else if (s_tintApplied) {
        apply_tint(0.0f);
        s_tintApplied = false;
    }

    update_embers(s_tint > 0.0f);
}

void draw_boss_rush_hardmode_embers() {
    if (s_tint <= 0.0f) {
        return;
    }
    cXyz eye;
    cXyz center;
    if (!ember_camera(eye, center)) {
        return;
    }
    J2DGrafContext* ctx = dComIfGp_getCurrentGrafPort();
    if (ctx == nullptr) {
        return;
    }
    ctx->setup2D();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);

    const f32 t = static_cast<f32>(s_emberTick);
    Vec screen;
    f32 size = 0.0f;
    f32 alpha = 0.0f;

    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    for (const Ember& e : s_embers) {
        if (!e.ash || !project_ember(e, eye, screen, size, alpha)) {
            continue;
        }
        const f32 rot = e.phase + t * e.spin;
        const f32 tumble = std::fabs(std::cos(e.phase * 2.0f + t * e.spin * 1.7f));
        const f32 rx = size * 1.5f;
        const f32 ry = size * 1.5f * (0.3f + 0.7f * tumble);
        draw_fan(screen.x, screen.y, rx, ry, rot,
                 GXColor{52, 42, 40, static_cast<u8>(190.0f * alpha)},
                 GXColor{28, 22, 22, 0}, kAshSegs);
    }

    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_SET);
    for (const Ember& e : s_embers) {
        if (e.ash || !project_ember(e, eye, screen, size, alpha)) {
            continue;
        }
        const f32 flicker = 0.65f + 0.35f * std::sin(t * 0.33f + e.phase * 3.0f);
        const f32 heat = e.heat * flicker;

        cXyz tailPos = e.pos - e.vel * kEmberTailTicks;
        Vec tail;
        mDoLib_project(&tailPos, &tail);
        if (tail.z < 400000.0f) {
            draw_streak(screen.x, screen.y, tail.x, tail.y, size * 0.8f,
                        GXColor{255, 130, 40, static_cast<u8>(140.0f * alpha * heat)},
                        GXColor{200, 40, 10, 0});
        }

        draw_fan(screen.x, screen.y, size * 4.0f, size * 4.0f, 0.0f,
                 GXColor{255, 75, 20, static_cast<u8>(80.0f * alpha * heat)},
                 GXColor{255, 40, 10, 0}, kGlowSegs);
        draw_fan(screen.x, screen.y, size * 1.3f, size * 1.3f, 0.0f,
                 GXColor{255, 245, 205, static_cast<u8>(255.0f * alpha * (0.5f + 0.5f * heat))},
                 GXColor{255, 150, 50, 0}, kCoreSegs);
    }

    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_RGBA4, 0);
}

void boss_rush_hardmode_bind_config(const ConfigService* config_svc, ModContext* mod_ctx,
                                    ConfigVarHandle handle) {
    s_configSvc = config_svc;
    s_modCtx = mod_ctx;
    s_configVar = handle;
}

void boss_rush_hardmode_set_enabled(bool enabled) {
    if (s_enabled == enabled) {
        return;
    }
    s_enabled = enabled;
    log_info(enabled ? "[HardMode] enabled" : "[HardMode] disabled");
}

void boss_rush_hardmode_toggle() {
    const bool next = !s_enabled;
    if (s_configSvc != nullptr && s_configVar != 0 && s_modCtx != nullptr) {
        s_configSvc->set_bool(s_modCtx, s_configVar, next);
    }
    boss_rush_hardmode_set_enabled(next);
}

bool boss_rush_hardmode_enabled() {
    return s_enabled;
}

bool boss_rush_hardmode_active() {
    return s_enabled && boss_rush_session_active();
}

bool boss_rush_hardmode_service_available() {
    return svc_actor_attribute != nullptr && s_resolverHandle != 0;
}

f32 boss_rush_hardmode_health_scale(const fopAc_ac_c* actor) {
    if (s_resolverHandle == 0) {
        return 1.0f;
    }
    const HardModeBoss* boss = boss_for_actor(actor);
    return boss != nullptr ? boss->health : 1.0f;
}

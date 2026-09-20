#include "damage_vignette.hpp"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/resource.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <webgpu/webgpu.h>

bool g_configDamageVignetteEnabled = false;
int g_configDamageVignetteIntensity = 50;

namespace {

constexpr u16 kLifeUnitsPerHeart = 4;
constexpr u16 kLowHealthLifeUnits = kLifeUnitsPerHeart + kLifeUnitsPerHeart / 2;

constexpr double kFlashDurationSeconds = 0.55;
constexpr double kPulsePeriodSeconds = 0.85;
constexpr float kPulseAmplitude = 0.5f;

const GfxService* s_gfx = nullptr;
const ResourceService* s_res = nullptr;
ModContext* s_ctx = nullptr;

GfxDrawTypeHandle s_drawType = 0;
GfxStageHookHandle s_stageHook = 0;
ResourceBuffer s_shaderSource = RESOURCE_BUFFER_INIT;
GfxDeviceInfo s_device = GFX_DEVICE_INFO_INIT;
GfxRenderTargetLayout s_sceneLayout = GFX_RENDER_TARGET_LAYOUT_INIT;
WGPURenderPipeline s_pipeline = nullptr;
WGPUBindGroupLayout s_bindGroupLayout = nullptr;
WGPUSampler s_sampler = nullptr;

double s_flashStartSeconds = -1.0e9;
u16 s_lastLife = 0xFFFF;
std::atomic<bool> s_previewRequest{false};

struct VignetteUniforms {
    float params[4];
};
static_assert(sizeof(VignetteUniforms) == 16);

struct DrawPayload {
    WGPUTextureView color;
    uint32_t uniform_offset;
    uint32_t uniform_size;
};
static_assert(sizeof(DrawPayload) <= GFX_INLINE_DRAW_PAYLOAD_SIZE);
static_assert(std::is_trivially_copyable_v<DrawPayload>);

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool gameplay_active() {
    return daAlink_getAlinkActorClass() != nullptr && !dComIfGp_isPauseFlag();
}

bool low_health_now(u16 life) {
    return life > 0 && life <= kLowHealthLifeUnits;
}

constexpr u16 kRampMaxStartUnits = 5 * kLifeUnitsPerHeart;
float low_health_level(u16 life) {
    const u16 maxLife = dComIfGs_getMaxLife();
    if (maxLife == 0 || life >= maxLife) {
        return 0.0f;
    }
    const u16 start = std::min<u16>(maxLife / 2, kRampMaxStartUnits);
    if (life >= start) {
        return 0.0f;
    }
    const float t = 1.0f - static_cast<float>(life) / static_cast<float>(start);
    return t * t * (3.0f - 2.0f * t);
}

double pulse_phase(double now) {
    return std::fmod(now, kPulsePeriodSeconds) / kPulsePeriodSeconds;
}

float vignette_intensity_now() {
    const float slider = static_cast<float>(g_configDamageVignetteIntensity) / 100.0f;
    const double now = now_seconds();

    float flash = 0.0f;
    const double flashT = (now - s_flashStartSeconds) / kFlashDurationSeconds;
    if (flashT >= 0.0 && flashT < 1.0) {
        flash = std::pow(static_cast<float>(1.0 - flashT), 1.7f);
    }

    float pulse = 0.0f;
    const u16 life = dComIfGs_getLife();
    if (low_health_now(life)) {
        const float bump = 0.5f - 0.5f * std::cos(static_cast<float>(
                                                pulse_phase(now) * 2.0 * 3.14159265358979));
        pulse = std::pow(bump, 1.6f) * kPulseAmplitude;
    }

    const float base = low_health_level(life) * slider;
    const float anim = std::min(flash + pulse, 1.0f) * slider;
    return std::min(base + anim, 1.0f);
}

bool ensure_pipeline();

void on_draw(ModContext*, const GfxDrawContext* ctx, const void* payload, size_t payloadSize,
             void*) {
    if (payloadSize != sizeof(DrawPayload) || ctx->layout.key != s_sceneLayout.key) {
        return;
    }
    DrawPayload data;
    std::memcpy(&data, payload, sizeof(data));
    if (data.color == nullptr || s_pipeline == nullptr) {
        return;
    }

    WGPUBindGroupEntry entries[3] = {
        WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = data.color;
    entries[1].binding = 1;
    entries[1].buffer = ctx->uniform_buffer;
    entries[1].offset = data.uniform_offset;
    entries[1].size = data.uniform_size;
    entries[2].binding = 2;
    entries[2].sampler = s_sampler;
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.layout = s_bindGroupLayout;
    desc.entryCount = 3;
    desc.entries = entries;
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(ctx->device, &desc);
    if (group == nullptr) {
        return;
    }

    wgpuRenderPassEncoderSetPipeline(ctx->pass, s_pipeline);
    wgpuRenderPassEncoderSetBindGroup(ctx->pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(ctx->pass, 3, 1, 0, 0);
    wgpuBindGroupRelease(group);
}

void on_stage_frame(ModContext*, const GfxStageContext*, void*) {
    if (s_gfx == nullptr || s_drawType == 0 || !g_configDamageVignetteEnabled) {
        return;
    }
    if (!gameplay_active()) {
        return;
    }

    const float intensity = vignette_intensity_now();
    if (intensity < 0.004f) {
        return;
    }

    if (!ensure_pipeline() || s_pipeline == nullptr) {
        return;
    }

    GfxResolveDesc resolveDesc = GFX_RESOLVE_DESC_INIT;
    resolveDesc.color = true;
    resolveDesc.depth = false;
    GfxResolvedTargets resolved = GFX_RESOLVED_TARGETS_INIT;
    if (s_gfx->resolve_pass(s_ctx, &resolveDesc, &resolved) != MOD_OK ||
        resolved.color == nullptr || resolved.width < 32 || resolved.height < 32)
    {
        return;
    }

    VignetteUniforms uniforms{};
    uniforms.params[0] = intensity;

    GfxRange uniformRange{0, 0};
    if (s_gfx->push_uniform(s_ctx, &uniforms, sizeof(uniforms), &uniformRange) != MOD_OK) {
        return;
    }
    const DrawPayload payload{resolved.color, uniformRange.offset, uniformRange.size};
    s_gfx->push_draw(s_ctx, s_drawType, &payload, sizeof(payload));
}

ModResult build_pipeline(ModError* error) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = {static_cast<const char*>(s_shaderSource.data), s_shaderSource.size};
    WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDesc.nextInChain = &wgsl.chain;
    moduleDesc.label = {"damage vignette", WGPU_STRLEN};
    WGPUShaderModule module = wgpuDeviceCreateShaderModule(s_device.device, &moduleDesc);
    if (module == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to compile damage_vignette.wgsl");
    }

    WGPUColorTargetState targets[GFX_MAX_COLOR_ATTACHMENTS];
    const uint32_t targetCount = gfx_init_color_target_states(&s_sceneLayout, targets, nullptr,
        WGPUColorWriteMask_Red | WGPUColorWriteMask_Green | WGPUColorWriteMask_Blue);

    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = module;
    fragment.entryPoint = {"fs_main", WGPU_STRLEN};
    fragment.targetCount = targetCount;
    fragment.targets = targets;

    WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
    depthStencil.format = s_sceneLayout.depth_stencil_format;
    depthStencil.depthWriteEnabled = WGPUOptionalBool_False;
    depthStencil.depthCompare = WGPUCompareFunction_Always;

    WGPURenderPipelineDescriptor desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    desc.label = {"damage vignette", WGPU_STRLEN};
    desc.vertex.module = module;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.depthStencil = &depthStencil;
    desc.multisample.count = s_sceneLayout.sample_count;
    desc.fragment = &fragment;

    s_pipeline = wgpuDeviceCreateRenderPipeline(s_device.device, &desc);
    wgpuShaderModuleRelease(module);
    if (s_pipeline == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to create damage vignette pipeline");
    }
    s_bindGroupLayout = wgpuRenderPipelineGetBindGroupLayout(s_pipeline, 0);
    if (s_bindGroupLayout == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to get damage vignette bind group layout");
    }
    return MOD_OK;
}

bool ensure_pipeline() {
    GfxRenderTargetLayout layout = GFX_RENDER_TARGET_LAYOUT_INIT;
    if (s_gfx->get_scene_target_layout(s_ctx, &layout) != MOD_OK) {
        return false;
    }
    if (s_pipeline != nullptr && layout.key == s_sceneLayout.key) {
        return true;
    }
    if (s_bindGroupLayout != nullptr) {
        wgpuBindGroupLayoutRelease(s_bindGroupLayout);
        s_bindGroupLayout = nullptr;
    }
    if (s_pipeline != nullptr) {
        wgpuRenderPipelineRelease(s_pipeline);
        s_pipeline = nullptr;
    }
    s_sceneLayout = layout;
    return build_pipeline(nullptr) == MOD_OK;
}

}

ModResult init_damage_vignette(const GfxService* gfx_svc, const ResourceService* res_svc,
                               const LogService* log_svc, ModContext* mod_ctx, ModError* error) {
    s_gfx = gfx_svc;
    s_res = res_svc;
    s_ctx = mod_ctx;
    if (s_gfx == nullptr || s_res == nullptr) {
        return MOD_OK;
    }

    ModResult result = s_res->load(s_ctx, "damage_vignette.wgsl", &s_shaderSource);
    if (result != MOD_OK || s_shaderSource.data == nullptr) {
        return mods::set_error(error, result != MOD_OK ? result : MOD_ERROR,
            "failed to load damage_vignette.wgsl");
    }

    if (s_gfx->get_device_info(s_ctx, &s_device) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "damage vignette: failed to query device info");
    }
    if (s_gfx->get_scene_target_layout(s_ctx, &s_sceneLayout) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "damage vignette: failed to query scene layout");
    }
    result = build_pipeline(error);
    if (result != MOD_OK) {
        return result;
    }

    WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDesc.label = {"Damage vignette linear sampler", WGPU_STRLEN};
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    s_sampler = wgpuDeviceCreateSampler(s_device.device, &samplerDesc);
    if (s_sampler == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to create damage vignette sampler");
    }

    GfxDrawTypeDesc drawDesc = GFX_DRAW_TYPE_DESC_INIT;
    drawDesc.label = "damage vignette";
    drawDesc.draw = on_draw;
    if (s_gfx->register_draw_type(s_ctx, &drawDesc, &s_drawType) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "damage vignette: failed to register draw type");
    }

    GfxStageHookDesc stageDesc = GFX_STAGE_HOOK_DESC_INIT;
    stageDesc.callback = on_stage_frame;
    if (s_gfx->register_stage_hook(
            s_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &stageDesc, &s_stageHook) != MOD_OK)
    {
        return mods::set_error(error, MOD_ERROR, "damage vignette: failed to register stage hook");
    }
    return MOD_OK;
}

void update_damage_vignette(const LogService*, ModContext*) {
    if (s_gfx == nullptr || !g_configDamageVignetteEnabled) {
        return;
    }
    const double now = now_seconds();

    if (s_previewRequest.exchange(false, std::memory_order_acq_rel)) {
        s_flashStartSeconds = now;
    }

    const u16 life = dComIfGs_getLife();
    if (s_lastLife != 0xFFFF && life < s_lastLife && gameplay_active()) {
        s_flashStartSeconds = now;
    }
    s_lastLife = life;
}

void damage_vignette_request_preview() {
    s_previewRequest.store(true, std::memory_order_release);
}

void shutdown_damage_vignette() {
    if (s_stageHook != 0 && s_gfx != nullptr) {
        s_gfx->unregister_stage_hook(s_ctx, s_stageHook);
    }
    if (s_drawType != 0 && s_gfx != nullptr) {
        s_gfx->unregister_draw_type(s_ctx, s_drawType);
    }
    if (s_pipeline != nullptr) {
        wgpuRenderPipelineRelease(s_pipeline);
        s_pipeline = nullptr;
    }
    if (s_bindGroupLayout != nullptr) {
        wgpuBindGroupLayoutRelease(s_bindGroupLayout);
        s_bindGroupLayout = nullptr;
    }
    if (s_sampler != nullptr) {
        wgpuSamplerRelease(s_sampler);
        s_sampler = nullptr;
    }
    if (s_res != nullptr && s_ctx != nullptr) {
        s_res->free(s_ctx, &s_shaderSource);
    }
    s_stageHook = 0;
    s_drawType = 0;
    s_gfx = nullptr;
    s_res = nullptr;
    s_ctx = nullptr;
    s_lastLife = 0xFFFF;
}

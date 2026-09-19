#include "oxygen_vignette.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
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

bool g_configOxygenVignetteEnabled = false;

namespace {

constexpr double kPreviewDurationSeconds = 0.9;
constexpr float kWarnBelowOxygenRatio = 0.5f;

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

float s_intensity = 0.0f;
float s_pulsePhase = 0.0f;
double s_previewStartSeconds = -1.0e9;
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
    if (dComIfGp_getPlayer(0) == nullptr) return false;
    if (dComIfGp_isPauseFlag() || dScnPly_c::isPause()) return false;
    return true;
}

float oxygen_vignette_intensity_now() {
    const float slider = static_cast<float>(g_configDamageVignetteIntensity) / 100.0f;
    const double now = now_seconds();

    const float wave = 0.55f + 0.45f * std::sin(s_pulsePhase);
    float level = s_intensity * (0.45f + 0.55f * wave);

    float preview = 0.0f;
    const double previewT = (now - s_previewStartSeconds) / kPreviewDurationSeconds;
    if (previewT >= 0.0 && previewT < 1.0) {
        preview = std::pow(static_cast<float>(1.0 - previewT), 1.7f);
    }

    return std::min(std::max(level, preview), 1.0f) * slider;
}

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
    if (s_gfx == nullptr || s_drawType == 0 || !g_configOxygenVignetteEnabled) {
        return;
    }
    if (!gameplay_active()) {
        return;
    }

    const float intensity = oxygen_vignette_intensity_now();
    if (intensity < 0.004f) {
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
    moduleDesc.label = {"oxygen vignette", WGPU_STRLEN};
    WGPUShaderModule module = wgpuDeviceCreateShaderModule(s_device.device, &moduleDesc);
    if (module == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to compile oxygen_vignette.wgsl");
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
    desc.label = {"oxygen vignette", WGPU_STRLEN};
    desc.vertex.module = module;
    desc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    desc.depthStencil = &depthStencil;
    desc.multisample.count = s_sceneLayout.sample_count;
    desc.fragment = &fragment;

    s_pipeline = wgpuDeviceCreateRenderPipeline(s_device.device, &desc);
    wgpuShaderModuleRelease(module);
    if (s_pipeline == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to create oxygen vignette pipeline");
    }
    s_bindGroupLayout = wgpuRenderPipelineGetBindGroupLayout(s_pipeline, 0);
    if (s_bindGroupLayout == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to get oxygen vignette bind group layout");
    }
    return MOD_OK;
}

}

ModResult init_oxygen_vignette(const GfxService* gfx_svc, const ResourceService* res_svc,
                               const LogService* log_svc, ModContext* mod_ctx, ModError* error) {
    (void)log_svc;
    s_gfx = gfx_svc;
    s_res = res_svc;
    s_ctx = mod_ctx;
    if (s_gfx == nullptr || s_res == nullptr) {
        return MOD_OK;
    }

    ModResult result = s_res->load(s_ctx, "oxygen_vignette.wgsl", &s_shaderSource);
    if (result != MOD_OK || s_shaderSource.data == nullptr) {
        return mods::set_error(error, result != MOD_OK ? result : MOD_ERROR,
            "failed to load oxygen_vignette.wgsl");
    }

    if (s_gfx->get_device_info(s_ctx, &s_device) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "oxygen vignette: failed to query device info");
    }
    if (s_gfx->get_scene_target_layout(s_ctx, &s_sceneLayout) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "oxygen vignette: failed to query scene layout");
    }
    result = build_pipeline(error);
    if (result != MOD_OK) {
        return result;
    }

    WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDesc.label = {"Oxygen vignette linear sampler", WGPU_STRLEN};
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    s_sampler = wgpuDeviceCreateSampler(s_device.device, &samplerDesc);
    if (s_sampler == nullptr) {
        return mods::set_error(error, MOD_ERROR, "failed to create oxygen vignette sampler");
    }

    GfxDrawTypeDesc drawDesc = GFX_DRAW_TYPE_DESC_INIT;
    drawDesc.label = "oxygen vignette";
    drawDesc.draw = on_draw;
    if (s_gfx->register_draw_type(s_ctx, &drawDesc, &s_drawType) != MOD_OK) {
        return mods::set_error(error, MOD_ERROR, "oxygen vignette: failed to register draw type");
    }

    GfxStageHookDesc stageDesc = GFX_STAGE_HOOK_DESC_INIT;
    stageDesc.callback = on_stage_frame;
    if (s_gfx->register_stage_hook(
            s_ctx, GFX_STAGE_FRAME_BEFORE_HUD, &stageDesc, &s_stageHook) != MOD_OK)
    {
        return mods::set_error(error, MOD_ERROR, "oxygen vignette: failed to register stage hook");
    }
    return MOD_OK;
}

void update_oxygen_vignette(const LogService*, ModContext*) {
    if (s_gfx == nullptr) {
        return;
    }
    const double now = now_seconds();

    if (s_previewRequest.exchange(false, std::memory_order_acq_rel)) {
        s_previewStartSeconds = now;
    }

    if (!g_configOxygenVignetteEnabled || !gameplay_active()) {
        s_intensity = 0.0f;
        return;
    }

    const int oxygen = dComIfGp_getOxygen();
    const s32 maxOxygen = dComIfGp_getMaxOxygen();

    float target = 0.0f;
    if (maxOxygen > 0 && oxygen < maxOxygen) {
        const float ratio = static_cast<float>(oxygen) / static_cast<float>(maxOxygen);
        if (ratio < kWarnBelowOxygenRatio) {
            target = (kWarnBelowOxygenRatio - ratio) / kWarnBelowOxygenRatio;
        } else if (ratio < 1.0f) {
            target = (1.0f - ratio) / (1.0f - kWarnBelowOxygenRatio) * 0.25f;
        }
        if (target > 1.0f) target = 1.0f;
    }

    s_intensity += (target - s_intensity) * (target > s_intensity ? 0.14f : 0.10f);
    if (s_intensity < 0.001f) s_intensity = 0.0f;
    if (s_intensity > 1.0f) s_intensity = 1.0f;

    const float urgency = target;
    s_pulsePhase += 0.0314f + urgency * 0.0942f;
    if (s_pulsePhase > 6.2831853f) s_pulsePhase -= 6.2831853f;
}

void oxygen_vignette_request_preview() {
    s_previewRequest.store(true, std::memory_order_release);
}

void shutdown_oxygen_vignette() {
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
    s_intensity = 0.0f;
    s_pulsePhase = 0.0f;
}

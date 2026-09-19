// Oxygen vignette - structured 1:1 after the damage vignette shader:
// fullscreen triangle, scene color snapshot + uniform + sampler in bind
// group 0, RGB-only overwrite so scene alpha survives. Instead of darkening
// the edges, they are mixed toward blue by the pushed intensity.

struct Uniforms {
    params: vec4<f32>, // x = intensity (0..1, animation baked in by the mod)
};

@group(0) @binding(0) var tex0: texture_2d<f32>;
@group(0) @binding(1) var<uniform> u: Uniforms;
@group(0) @binding(2) var linear_sampler: sampler;

struct VSOut {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

// Fullscreen triangle in NDC; framebuffer v is flipped (uv.y = 0 at the top).
@vertex
fn vs_main(@builtin(vertex_index) vi: u32) -> VSOut {
    var positions = array<vec2<f32>, 3>(vec2<f32>(-1.0, -1.0), vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0));
    let p = positions[vi];
    var out: VSOut;
    out.pos = vec4<f32>(p, 0.0, 1.0);
    out.uv = p.xy * vec2<f32>(0.5, -0.5) + vec2<f32>(0.5);
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let uv = clamp(in.uv, vec2<f32>(0.0), vec2<f32>(1.0));
    let c = textureSampleLevel(tex0, linear_sampler, uv, 0.0).rgb;

    // Vignette falloff, same convention as the shader pack's grade vignette:
    // d = distance(uv, center) * sqrt(2); corners reach 1.0.
    let d = distance(uv, vec2<f32>(0.5, 0.5)) * 1.4142;
    let mask = smoothstep(0.45, 1.1, d) * u.params.x;

    let blue = vec3<f32>(0.25, 0.58, 1.0);
    let out = mix(c, blue, clamp(mask, 0.0, 1.0));
    return vec4<f32>(out, 0.0);
}

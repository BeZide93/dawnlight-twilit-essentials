struct Uniforms {
    params: vec4<f32>,
};

@group(0) @binding(0) var tex0: texture_2d<f32>;
@group(0) @binding(1) var<uniform> u: Uniforms;
@group(0) @binding(2) var linear_sampler: sampler;

struct VSOut {
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

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

const GOLDEN_ANGLE: f32 = 2.39996323;
const BLUR_TAPS: f32 = 24.0;

fn hash12(p: vec2<f32>) -> f32 {
    var p3 = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

fn sample_rgb(uv: vec2<f32>) -> vec3<f32> {
    return textureSampleLevel(tex0, linear_sampler, clamp(uv, vec2<f32>(0.0), vec2<f32>(1.0)), 0.0).rgb;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let uv = clamp(in.uv, vec2<f32>(0.0), vec2<f32>(1.0));
    let level = clamp(u.params.x, 0.0, 1.0);
    let focusHalfWidth = u.params.y;
    let feather = max(u.params.z, 0.001);
    let darken = u.params.w;

    let dims = vec2<f32>(textureDimensions(tex0));
    let texel = 1.0 / max(dims, vec2<f32>(1.0));

    let dx = abs(uv.x - 0.5);
    let mask = smoothstep(focusHalfWidth, focusHalfWidth + feather, dx) * level;
    let blurRadius = mask * dims.y * 0.0085;

    var col = vec3<f32>(0.0);
    if (blurRadius > 0.35) {
        let spin = hash12(in.pos.xy) * 6.2831853;
        var acc = vec3<f32>(0.0);
        for (var i = 0.0; i < BLUR_TAPS; i += 1.0) {
            let a = i * GOLDEN_ANGLE + spin;
            let rad = sqrt((i + 0.5) / BLUR_TAPS) * blurRadius;
            acc += sample_rgb(uv + vec2<f32>(cos(a), sin(a)) * rad * texel);
        }
        col = acc / BLUR_TAPS;
    } else {
        col = sample_rgb(uv);
    }

    col *= 1.0 - mask * darken;
    return vec4<f32>(clamp(col, vec3<f32>(0.0), vec3<f32>(1.0)), 0.0);
}

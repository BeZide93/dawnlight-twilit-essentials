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
const BLUR_TAPS: f32 = 12.0;

fn hash12(p: vec2<f32>) -> f32 {
    var p3 = fract(vec3<f32>(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

fn sample_rgb(uv: vec2<f32>) -> vec3<f32> {
    return textureSampleLevel(tex0, linear_sampler, clamp(uv, vec2<f32>(0.0), vec2<f32>(1.0)), 0.0).rgb;
}

fn heartbeat(time: f32) -> f32 {
    let t = fract(time / 1.3);
    let first = exp(-t * 16.0);
    let second = step(0.2, t) * exp(-(t - 0.2) * 16.0) * 0.6;
    return first + second;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let uv = clamp(in.uv, vec2<f32>(0.0), vec2<f32>(1.0));
    let intensity = clamp(u.params.x, 0.0, 1.0);
    let time = u.params.y;
    let ripple = clamp(u.params.z, 0.0, 1.0);
    let rippleAlpha = clamp(u.params.w, 0.0, 1.0);

    let dims = vec2<f32>(textureDimensions(tex0));
    let texel = 1.0 / max(dims, vec2<f32>(1.0));
    let aspect = dims.x / max(dims.y, 1.0);
    let center = vec2<f32>(0.5, 0.5);
    let dir = uv - center;
    let r = length(vec2<f32>(dir.x * aspect, dir.y)) / length(vec2<f32>(aspect * 0.5, 0.5));
    let radial = dir / max(length(dir), 1e-4);

    let barrel = 1.0 - 0.05 * intensity * r * r;
    var suv = center + dir * barrel;

    let ringDist = r - ripple * 1.35;
    let ring = exp(-ringDist * ringDist * 180.0) * rippleAlpha;
    suv -= radial * ring * 0.02;

    let focus = smoothstep(0.28, 1.0, r) * intensity;
    let blurRadius = focus * 7.0;
    var col = vec3<f32>(0.0);
    if (blurRadius > 0.25) {
        let spin = hash12(in.pos.xy) * 6.2831853;
        var acc = vec3<f32>(0.0);
        for (var i = 0.0; i < BLUR_TAPS; i += 1.0) {
            let a = i * GOLDEN_ANGLE + spin;
            let rad = sqrt((i + 0.5) / BLUR_TAPS) * blurRadius;
            acc += sample_rgb(suv + vec2<f32>(cos(a), sin(a)) * rad * texel);
        }
        col = acc / BLUR_TAPS;
    } else {
        col = sample_rgb(suv);
    }

    if (ring > 0.01) {
        let split = radial * ring * 0.012;
        col.r = mix(col.r, sample_rgb(suv + split).r, ring);
        col.b = mix(col.b, sample_rgb(suv - split).b, ring);
    }

    let lum = dot(col, vec3<f32>(0.2126, 0.7152, 0.0722));
    let desat = (0.25 + 0.6 * smoothstep(0.15, 0.9, r)) * intensity;
    col = mix(col, vec3<f32>(lum), desat);

    let shadowTint = vec3<f32>(0.62, 0.66, 0.92);
    let highTint = vec3<f32>(1.1, 0.96, 0.74);
    let tone = mix(shadowTint, highTint, smoothstep(0.05, 0.75, lum));
    col = mix(col, col * tone, 0.85 * intensity);
    col = mix(col, (col - vec3<f32>(0.5)) * 1.1 + vec3<f32>(0.5), intensity);

    let beat = heartbeat(time) * intensity;
    let vig = smoothstep(0.5, 1.3, r + beat * 0.05);
    col *= 1.0 - vig * (0.72 + beat * 0.1) * intensity;

    col += vec3<f32>(1.0, 0.82, 0.5) * ring * 0.22;

    let grain = hash12(floor(in.pos.xy) + vec2<f32>(floor(time * 24.0) * 37.0, 0.0)) - 0.5;
    col += vec3<f32>(grain) * 0.04 * intensity * (0.4 + focus);

    return vec4<f32>(clamp(col, vec3<f32>(0.0), vec3<f32>(1.0)), 0.0);
}

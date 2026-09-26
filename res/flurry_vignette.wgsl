// Flurry Rush edge blur - structured 1:1 after damage_vignette.wgsl
// (fullscreen triangle, scene color snapshot + uniform + sampler in bind
// group 0, RGB-only overwrite so scene alpha survives). Effect: a radial
// (zoom) blur that strengthens toward the screen edges while the flurry-rush
// slow motion runs - the center stays sharp, the edges streak outward and the
// whole frame pushes in slightly, like a BotW flurry-rush camera zoom.
// On top of the blur the frame gets a flurry-rush look that keeps its colors:
// slightly richer saturation with a cool blue tint, chromatic aberration and a
// glowing cyan rim toward the edges, flickering radial speed lines and a soft
// blue-white flash when the rush starts.
// params.x = effect strength (0..1, animation baked in by the mod),
// params.y = push-in zoom (>= 1.0), params.z = time in seconds (speed-line
// flicker), params.w = start flash (0..1).

struct Uniforms {
    params: vec4<f32>, // x = intensity, y = push-in zoom, z = time, w = flash
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

const PI: f32 = 3.14159265;
const SPEED_LINE_COUNT: f32 = 150.0;

fn hash11(n: f32) -> f32 {
    return fract(sin(n * 12.9898) * 43758.5453);
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let uv = clamp(in.uv, vec2<f32>(0.0), vec2<f32>(1.0));
    let intensity = clamp(u.params.x, 0.0, 1.0);
    let center = vec2<f32>(0.5, 0.5);
    let dir = uv - center;

    // Radial falloff: 0 in the center (Link stays sharp), 1 well outside the
    // middle third. Same distance convention as the damage vignette.
    let d = distance(uv, center) * 1.4142;
    let blurStrength = smoothstep(0.26, 0.92, d) * intensity;

    // Chromatic aberration: red and blue split along the radius at the edges.
    let ca = 0.007 * intensity * smoothstep(0.2, 1.0, d);
    var col = vec3<f32>(
        textureSampleLevel(tex0, linear_sampler, clamp(uv + dir * ca, vec2<f32>(0.0), vec2<f32>(1.0)), 0.0).r,
        textureSampleLevel(tex0, linear_sampler, uv, 0.0).g,
        textureSampleLevel(tex0, linear_sampler, clamp(uv - dir * ca, vec2<f32>(0.0), vec2<f32>(1.0)), 0.0).b);

    if (blurStrength > 0.01) {
        // Total pull-in at the pixel: the camera push-in plus a blur-only
        // spread that grows with the radial falloff, so the edges streak
        // farther than the frame pushes in.
        let zoom = max(u.params.y, 1.0);
        let zoomFull = zoom * (1.0 + 0.35 * blurStrength);

        // Zoom blur: accumulate samples along the center->pixel ray, each at
        // a different pull-in from none to full. The center pixel's ray is
        // nearly zero long, which keeps it sharp without a special case.
        let samples = 8.0;
        var acc = vec3<f32>(0.0);
        var total = 0.0;
        for (var i = 0.0; i < samples; i += 1.0) {
            let t = i / (samples - 1.0);
            let z = 1.0 + (zoomFull - 1.0) * t;
            let suv = clamp(center + dir / z, vec2<f32>(0.0), vec2<f32>(1.0));
            acc += textureSampleLevel(tex0, linear_sampler, suv, 0.0).rgb;
            total += 1.0;
        }
        let blurred = acc / total;

        // Fade the blur in with the same radial curve, keeping a little of
        // the sharp base color so edges darken slightly as they dissolve.
        col = mix(col, blurred, clamp(blurStrength * 1.2, 0.0, 1.0));

        // Strong pull-ins sample beyond the frame bounds; clamping smears the
        // border texels - dim them so the frame edge reads as dissolving into
        // the blur instead of repeating.
        let outer = clamp(center + dir / zoomFull, vec2<f32>(0.0), vec2<f32>(1.0));
        let outside = max(max(vec2<f32>(0.0) - outer, outer - vec2<f32>(1.0)), vec2<f32>(0.0));
        let outsideMask = clamp(max(outside.x, outside.y) * 30.0, 0.0, 1.0) * blurStrength;
        col = mix(col, col * 0.5, outsideMask);
    }

    // Keep the colors: a little more saturation and a cool blue tint.
    let lum = dot(col, vec3<f32>(0.299, 0.587, 0.114));
    col = mix(vec3<f32>(lum), col, 1.0 + 0.25 * intensity);
    col *= mix(vec3<f32>(1.0), vec3<f32>(0.9, 1.0, 1.14), intensity);

    // Edges dim slightly and then glow cyan.
    let rim = smoothstep(0.5, 1.1, d);
    col = mix(col, col * vec3<f32>(0.6, 0.7, 0.85), rim * 0.45 * intensity);
    col += vec3<f32>(0.08, 0.32, 0.55) * rim * 0.5 * intensity;

    // Radial speed lines: thin angular slots, re-rolled 12 times a second,
    // only toward the edges.
    let slotPos = (atan2(dir.y, dir.x) + PI) / (2.0 * PI) * SPEED_LINE_COUNT;
    let slot = floor(slotPos);
    let roll = hash11(slot * 1.73 + floor(u.params.z * 12.0) * 17.31);
    let profile = 1.0 - abs(fract(slotPos) - 0.5) * 2.0;
    let speedLine = step(0.86, roll) * smoothstep(0.55, 1.0, profile) * smoothstep(0.5, 1.0, d);
    col += vec3<f32>(0.75, 0.9, 1.0) * speedLine * 0.5 * intensity;

    col = mix(col, vec3<f32>(0.85, 0.95, 1.0), clamp(u.params.w, 0.0, 1.0) * 0.6);

    return vec4<f32>(clamp(col, vec3<f32>(0.0), vec3<f32>(1.0)), 0.0);
}

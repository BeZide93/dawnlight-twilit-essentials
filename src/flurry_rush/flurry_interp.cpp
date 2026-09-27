#include "flurry_interp.hpp"

#include "mods/svc/hook.hpp"

#include <cmath>
#include <cstdint>

bool flurry_rush_is_rush_active();

namespace {

using Mtx34 = float[3][4];
using StepFn = float (*)();

StepFn s_getStep = nullptr;

DEFINE_HOOK_SYMBOL("dusk::interp::lookup_replacement", bool(const void*, float (*)[4]),
                   FlurryLookupReplacementHook);
DEFINE_HOOK_SYMBOL("dusk::interp::lookup_concat_replacement",
                   bool(const void*, const void*, float (*)[4]), FlurryLookupConcatHook);

constexpr float kMinStep = 0.02f;
constexpr float kMaxStep = 0.98f;
constexpr float kShrinkTolerance = 0.97f;
constexpr float kMaxSkew = 0.1f;
constexpr float kMaxScaleRatio = 2.0f;

struct Quat {
    float x, y, z, w;
};

float column_length(const Mtx34& m, int col) {
    return std::sqrt(m[0][col] * m[0][col] + m[1][col] * m[1][col] + m[2][col] * m[2][col]);
}

bool decompose(const Mtx34& m, Quat& q, float scale[3]) {
    float c[3][3];
    for (int j = 0; j < 3; j++) {
        const float len = column_length(m, j);
        if (!(len > 1e-6f)) return false;
        scale[j] = len;
        for (int i = 0; i < 3; i++) c[j][i] = m[i][j] / len;
    }

    const float skew01 = c[0][0] * c[1][0] + c[0][1] * c[1][1] + c[0][2] * c[1][2];
    const float skew02 = c[0][0] * c[2][0] + c[0][1] * c[2][1] + c[0][2] * c[2][2];
    const float skew12 = c[1][0] * c[2][0] + c[1][1] * c[2][1] + c[1][2] * c[2][2];
    if (std::fabs(skew01) > kMaxSkew || std::fabs(skew02) > kMaxSkew || std::fabs(skew12) > kMaxSkew) {
        return false;
    }

    for (int i = 0; i < 3; i++) c[1][i] -= skew01 * c[0][i];
    const float len1 = std::sqrt(c[1][0] * c[1][0] + c[1][1] * c[1][1] + c[1][2] * c[1][2]);
    if (!(len1 > 1e-6f)) return false;
    for (int i = 0; i < 3; i++) c[1][i] /= len1;

    const float cross[3] = {
        c[0][1] * c[1][2] - c[0][2] * c[1][1],
        c[0][2] * c[1][0] - c[0][0] * c[1][2],
        c[0][0] * c[1][1] - c[0][1] * c[1][0],
    };
    if (cross[0] * c[2][0] + cross[1] * c[2][1] + cross[2] * c[2][2] <= 0.0f) return false;
    for (int i = 0; i < 3; i++) c[2][i] = cross[i];

    const float r00 = c[0][0], r01 = c[1][0], r02 = c[2][0];
    const float r10 = c[0][1], r11 = c[1][1], r12 = c[2][1];
    const float r20 = c[0][2], r21 = c[1][2], r22 = c[2][2];
    const float trace = r00 + r11 + r22;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s, 0.25f * s};
    } else if (r00 > r11 && r00 > r22) {
        const float s = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
        q = {0.25f * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s};
    } else if (r11 > r22) {
        const float s = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
        q = {(r01 + r10) / s, 0.25f * s, (r12 + r21) / s, (r02 - r20) / s};
    } else {
        const float s = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
        q = {(r02 + r20) / s, (r12 + r21) / s, 0.25f * s, (r10 - r01) / s};
    }
    return true;
}

Quat slerp(Quat a, Quat b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    float wa, wb;
    if (dot > 0.9995f) {
        wa = 1.0f - t;
        wb = t;
    } else {
        const float theta = std::acos(dot);
        const float sinTheta = std::sin(theta);
        wa = std::sin((1.0f - t) * theta) / sinTheta;
        wb = std::sin(t * theta) / sinTheta;
    }
    Quat r = {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
    const float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    if (len > 1e-6f) {
        r = {r.x / len, r.y / len, r.z / len, r.w / len};
    }
    return r;
}

bool scales_compatible(const float a[3], const float b[3]) {
    for (int j = 0; j < 3; j++) {
        if (a[j] > b[j] * kMaxScaleRatio || b[j] > a[j] * kMaxScaleRatio) return false;
    }
    return true;
}

bool rotation_shrunk(const Mtx34& lerped, const Mtx34& prev, const Mtx34& cur, float t) {
    for (int j = 0; j < 3; j++) {
        const float expected = column_length(prev, j) + (column_length(cur, j) - column_length(prev, j)) * t;
        if (column_length(lerped, j) < expected * kShrinkTolerance) return true;
    }
    return false;
}

bool fix_interpolated(float (*out)[4], const float (*cur)[4]) {
    if (s_getStep == nullptr) return false;
    const float t = s_getStep();
    if (!(t > kMinStep && t < kMaxStep)) return false;

    Mtx34 lerped;
    Mtx34 current;
    Mtx34 prev;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            lerped[i][j] = out[i][j];
            current[i][j] = cur[i][j];
            prev[i][j] = (lerped[i][j] - t * current[i][j]) / (1.0f - t);
        }
    }

    if (!rotation_shrunk(lerped, prev, current, t)) return false;

    Quat qPrev, qCur;
    float sPrev[3], sCur[3];
    if (!decompose(prev, qPrev, sPrev) || !decompose(current, qCur, sCur)) return false;
    if (!scales_compatible(sPrev, sCur)) return false;

    const Quat q = slerp(qPrev, qCur, t);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    const float rot[3][3] = {
        {1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz), 2.0f * (xz + wy)},
        {2.0f * (xy + wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx)},
        {2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (xx + yy)},
    };
    float scale[3];
    for (int j = 0; j < 3; j++) scale[j] = sPrev[j] + (sCur[j] - sPrev[j]) * t;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) out[i][j] = rot[i][j] * scale[j];
        out[i][3] = lerped[i][3];
    }
    return true;
}

void concat(const float (*a)[4], const float (*b)[4], float (*out)[4]) {
    Mtx34 r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
        r[i][3] += a[i][3];
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) out[i][j] = r[i][j];
    }
}

bool is_matrix_key(const void* key) {
    return key != nullptr && (reinterpret_cast<std::uintptr_t>(key) & 3) == 0;
}

void on_lookup_replacement_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || !*static_cast<bool*>(retval) || !flurry_rush_is_rush_active()) return;
    const void* key = mods::arg<const void*>(args, 0);
    float (*out)[4] = mods::arg<float (*)[4]>(args, 1);
    if (!is_matrix_key(key) || out == nullptr) return;
    fix_interpolated(out, static_cast<const float (*)[4]>(key));
}

void on_lookup_concat_post(ModContext*, void* args, void* retval, void*) {
    if (retval == nullptr || !*static_cast<bool*>(retval) || !flurry_rush_is_rush_active()) return;
    if (FlurryLookupReplacementHook::g_orig == nullptr) return;
    const void* lhs = mods::arg<const void*>(args, 0);
    const void* rhs = mods::arg<const void*>(args, 1);
    float (*out)[4] = mods::arg<float (*)[4]>(args, 2);
    if (!is_matrix_key(lhs) || !is_matrix_key(rhs) || out == nullptr) return;

    Mtx34 right;
    if (!FlurryLookupReplacementHook::g_orig(rhs, right)) return;
    if (!fix_interpolated(right, static_cast<const float (*)[4]>(rhs))) return;

    Mtx34 left;
    const float (*leftPtr)[4] = static_cast<const float (*)[4]>(lhs);
    if (FlurryLookupReplacementHook::g_orig(lhs, left)) leftPtr = left;
    concat(leftPtr, right, out);
}

}

void flurry_interp_resolve(const HookService* hook_svc) {
    if (hook_svc == nullptr || hook_svc->resolve == nullptr) return;
    if (hook_svc->resolve(mod_ctx, "dusk::interp::get_interpolation_step",
                          reinterpret_cast<void**>(&s_getStep), nullptr) != MOD_OK)
    {
        s_getStep = nullptr;
    }
}

void flurry_interp_install(const HookService* hook_svc) {
    if (hook_svc == nullptr || s_getStep == nullptr) return;
    mods::hook::add_post<FlurryLookupReplacementHook>(hook_svc, on_lookup_replacement_post);
    mods::hook::add_post<FlurryLookupConcatHook>(hook_svc, on_lookup_concat_post);
}


#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#include <vector>

#include <mods/svc/host.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

static constexpr u32 kGX_TF_RGBA8 = 0x06;
static constexpr size_t kResTIMGSize = 0x20;
static constexpr size_t kMaxPngBytes = 64 * 1024 * 1024;
static constexpr int kMaxTextureDim = 8192;

static void tex_rep_log(const char* fmt, ...) {
    if (g_logSvc == nullptr || g_modCtx == nullptr) return;
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    g_logSvc->info(g_modCtx, buffer);
}

static int tex_rep_timg_width(const ResTIMG* img) {
    const u8* p = reinterpret_cast<const u8*>(img);
    return (p[0x02] << 8) | p[0x03];
}

static int tex_rep_timg_height(const ResTIMG* img) {
    const u8* p = reinterpret_cast<const u8*>(img);
    return (p[0x04] << 8) | p[0x05];
}

static void tex_rep_write_be16(u8* p, int v) {
    p[0] = static_cast<u8>(v >> 8);
    p[1] = static_cast<u8>(v);
}

static void tex_rep_write_be32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

static bool tex_rep_resolve_png_path(const char* resPath, std::filesystem::path& out) {
    const HostService* host = cl_host_service();
    if (host == nullptr || g_modCtx == nullptr ||
        !SERVICE_HAS(host, HostService, data_dir)) {
        return false;
    }

    const char* dataDir = nullptr;
    if (host->data_dir(g_modCtx, &dataDir) != MOD_OK || dataDir == nullptr) {
        return false;
    }

    std::error_code ec;
    const std::filesystem::path configDir =
        std::filesystem::path(dataDir).parent_path().parent_path();

    const char* slash = std::strrchr(resPath, '/');
    const char* base = (slash != nullptr) ? slash + 1 : resPath;
    std::string stem(base);
    const size_t dot = stem.rfind('.');
    if (dot != std::string::npos) {
        stem.resize(dot);
    }

    out = configDir / "texture_replacements" / (stem + ".png");
    return true;
}

static std::vector<u8> tex_rep_scale_bilinear(const std::vector<u8>& src, int srcW, int srcH,
                                              int dstW, int dstH) {
    std::vector<u8> dst(static_cast<size_t>(dstW) * dstH * 4);
    const float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
    const float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);
    for (int y = 0; y < dstH; ++y) {
        const float fy = std::max(0.0f, (y + 0.5f) * scaleY - 0.5f);
        const int y0 = std::min(static_cast<int>(fy), srcH - 1);
        const int y1 = std::min(y0 + 1, srcH - 1);
        const float ty = fy - static_cast<float>(y0);
        for (int x = 0; x < dstW; ++x) {
            const float fx = std::max(0.0f, (x + 0.5f) * scaleX - 0.5f);
            const int x0 = std::min(static_cast<int>(fx), srcW - 1);
            const int x1 = std::min(x0 + 1, srcW - 1);
            const float tx = fx - static_cast<float>(x0);

            const u8* p00 = &src[(static_cast<size_t>(y0) * srcW + x0) * 4];
            const u8* p10 = &src[(static_cast<size_t>(y0) * srcW + x1) * 4];
            const u8* p01 = &src[(static_cast<size_t>(y1) * srcW + x0) * 4];
            const u8* p11 = &src[(static_cast<size_t>(y1) * srcW + x1) * 4];
            u8* out = &dst[(static_cast<size_t>(y) * dstW + x) * 4];
            for (int c = 0; c < 4; ++c) {
                const float top = p00[c] + (p10[c] - p00[c]) * tx;
                const float bottom = p01[c] + (p11[c] - p01[c]) * tx;
                out[c] = static_cast<u8>(top + (bottom - top) * ty + 0.5f);
            }
        }
    }
    return dst;
}

static void tex_rep_encode_rgba8(u8* dst, const u8* src, int w, int h) {
    for (int ty = 0; ty < h / 4; ++ty) {
        for (int tx = 0; tx < w / 4; ++tx) {
            u8* tile = dst + (static_cast<size_t>(ty) * (w / 4) + tx) * 64;
            u8* ar = tile;
            u8* gb = tile + 32;
            for (int row = 0; row < 4; ++row) {
                const u8* line = src + (static_cast<size_t>(ty * 4 + row) * w + tx * 4) * 4;
                for (int col = 0; col < 4; ++col) {
                    ar[0] = line[3];
                    ar[1] = line[0];
                    gb[0] = line[1];
                    gb[1] = line[2];
                    ar += 2;
                    gb += 2;
                    line += 4;
                }
            }
        }
    }
}

static ResTIMG* tex_rep_build_override(ResTIMG* original, const u8* pngPixels, int pngW, int pngH) {
    const int targetW = tex_rep_timg_width(original);
    const int targetH = tex_rep_timg_height(original);
    if (targetW <= 0 || targetH <= 0 || targetW > kMaxTextureDim || targetH > kMaxTextureDim ||
        (targetW % 4) != 0 || (targetH % 4) != 0) {
        return nullptr;
    }

    std::vector<u8> rgba;
    int srcW = pngW;
    int srcH = pngH;
    if (pngW != targetW || pngH != targetH) {
        rgba = tex_rep_scale_bilinear(
            std::vector<u8>(pngPixels, pngPixels + static_cast<size_t>(pngW) * pngH * 4), pngW,
            pngH, targetW, targetH);
        srcW = targetW;
        srcH = targetH;
    } else {
        rgba.assign(pngPixels, pngPixels + static_cast<size_t>(pngW) * pngH * 4);
    }

    const size_t texelBytes = static_cast<size_t>(targetW) * targetH * 4;
    u8* buffer = nullptr;
    try {
        buffer = static_cast<u8*>(operator new[](kResTIMGSize + texelBytes, std::align_val_t(32)));
    } catch (...) {
        return nullptr;
    }

    u8 header[kResTIMGSize];
    std::memcpy(header, original, kResTIMGSize);
    header[0x00] = kGX_TF_RGBA8;
    header[0x01] = 1;
    tex_rep_write_be16(header + 0x02, targetW);
    tex_rep_write_be16(header + 0x04, targetH);
    header[0x08] = 0;
    header[0x09] = 0;
    tex_rep_write_be16(header + 0x0A, 0);
    tex_rep_write_be32(header + 0x0C, 0);
    header[0x10] = 0;
    header[0x18] = 1;
    tex_rep_write_be32(header + 0x1C, static_cast<u32>(kResTIMGSize));

    std::vector<u8> texels(texelBytes);
    tex_rep_encode_rgba8(texels.data(), rgba.data(), srcW, srcH);
    std::memcpy(buffer, header, kResTIMGSize);
    std::memcpy(buffer + kResTIMGSize, texels.data(), texelBytes);
    return reinterpret_cast<ResTIMG*>(buffer);
}

ResTIMG* tex_replacements_apply(const char* resPath, ResTIMG* fallback) {
    if (fallback == nullptr) {
        return nullptr;
    }

    std::filesystem::path pngPath;
    std::error_code ec;
    if (!tex_rep_resolve_png_path(resPath, pngPath) ||
        !std::filesystem::exists(pngPath, ec) || ec) {
        return fallback;
    }

    const auto fileSize = std::filesystem::file_size(pngPath, ec);
    if (ec || fileSize == 0 || fileSize > kMaxPngBytes) {
        return fallback;
    }

    std::ifstream file(pngPath, std::ios::binary);
    if (!file) {
        return fallback;
    }
    std::vector<u8> bytes(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(fileSize))) {
        return fallback;
    }

    int pngW = 0;
    int pngH = 0;
    int channels = 0;
    u8* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &pngW, &pngH,
                                       &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        tex_rep_log("texture replacement '%s': failed to decode PNG (%s)", pngPath.string().c_str(),
                    stbi_failure_reason());
        return fallback;
    }

    ResTIMG* overrideTex = tex_rep_build_override(fallback, pixels, pngW, pngH);
    stbi_image_free(pixels);
    if (overrideTex == nullptr) {
        tex_rep_log("texture replacement '%s': could not build %dx%d RGBA8 texture",
                    pngPath.string().c_str(), tex_rep_timg_width(fallback),
                    tex_rep_timg_height(fallback));
        return fallback;
    }

    tex_rep_log("texture replacement '%s' applied (%dx%d)", pngPath.string().c_str(),
                tex_rep_timg_width(overrideTex), tex_rep_timg_height(overrideTex));
    return overrideTex;
}

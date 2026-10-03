// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace xmb {
namespace {

constexpr uint8_t kTexDimMarker[] = {0x00, 0x80, 0x00, 0x80, 0x00, 0x08};
constexpr uint8_t kGimMagic[] = {'M', 'I', 'G', 0}; // reversed GIM in some blobs

bool canRead(const std::vector<uint8_t> &d, size_t off, size_t n)
{
    return off <= d.size() && n <= d.size() - off;
}

int findBytes(const std::vector<uint8_t> &d, const uint8_t *pat, size_t n, size_t start = 0)
{
    if (n == 0 || start >= d.size())
        return -1;
    for (size_t i = start; i + n <= d.size(); ++i) {
        if (std::memcmp(d.data() + i, pat, n) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

std::filesystem::path invertConfigPath()
{
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".config" / "vlnky" / "invert_textures.txt";
}

std::vector<uint8_t> normalizeBlock(std::vector<uint8_t> block, bool allowInvert, bool forceInvert, bool skipInvert)
{
    if (block.empty())
        return block;
    double sum = 0;
    for (uint8_t b : block)
        sum += b;
    const bool wouldInvert = sum / block.size() > 180.0;
    if (forceInvert || (allowInvert && !skipInvert && wouldInvert)) {
        for (auto &b : block)
            b = uint8_t(255 - b);
    }
    return block;
}

/// PSP 8bpp 128×128 swizzle → linear (GE texture order).
void unswizzle128x8(const uint8_t *swizzled, uint8_t *linear)
{
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const int blockX = x / 8;
            const int blockY = y / 8;
            const int inBlockX = x % 8;
            const int inBlockY = y % 8;
            const int blockIndex = blockY * 16 + blockX;
            const int pixelInBlock = inBlockY * 8 + inBlockX;
            const int swiz = blockIndex * 64 + pixelInBlock;
            linear[y * 128 + x] = swizzled[swiz];
        }
    }
}

bool tryBlock(const std::vector<uint8_t> &data, int w, int h, size_t imgOff, std::vector<uint8_t> &out)
{
    const size_t need = size_t(w) * size_t(h);
    if (imgOff + need > data.size())
        return false;
    std::vector<uint8_t> block(data.begin() + imgOff, data.begin() + imgOff + need);
    if (block.empty())
        return false;
    std::array<bool, 256> seen{};
    size_t uniq = 0;
    for (size_t i = 0; i < std::min(block.size(), size_t(64)); ++i) {
        if (!seen[block[i]]) {
            seen[block[i]] = true;
            ++uniq;
        }
    }
    if (uniq < 4)
        return false;
    out = std::move(block);
    return true;
}

TextureExtractResult tryGim(const std::vector<uint8_t> &data)
{
    TextureExtractResult r;
    const int gimOff = findBytes(data, kGimMagic, 4);
    if (gimOff < 0)
        return r;

    for (int w : {128, 256}) {
        for (int h : {128, 64}) {
            const size_t need = size_t(w) * size_t(h);
            for (size_t off = size_t(gimOff) + 4; off + need <= data.size() && off < size_t(gimOff) + 4096; off += 16) {
                std::vector<uint8_t> swizzled(data.begin() + off, data.begin() + off + need);
                if (w == 128 && h == 128) {
                    std::vector<uint8_t> linear(need);
                    unswizzle128x8(swizzled.data(), linear.data());
                    r.pixels = std::move(linear);
                } else {
                    r.pixels = std::move(swizzled);
                }
                r.width = w;
                r.height = h;
                r.method = "gim";
                return r;
            }
        }
    }
    return r;
}

TextureExtractResult tryTga(const std::vector<uint8_t> &data)
{
    TextureExtractResult r;
    for (size_t off = 0; off + 18 < data.size(); ++off) {
        if (!canRead(data, off, 18) || data[off + 2] != 2)
            continue;
        uint16_t w = 0, h = 0;
        std::memcpy(&w, data.data() + off + 12, 2);
        std::memcpy(&h, data.data() + off + 14, 2);
        const uint8_t bpp = data[off + 16];
        if (bpp != 8 || w < 64 || w > 256 || h < 32 || h > 256)
            continue;
        const size_t imgSize = size_t(w) * size_t(h);
        const size_t imgOff = off + 18;
        if (imgOff + imgSize > data.size())
            continue;
        r.pixels.assign(data.begin() + imgOff, data.begin() + imgOff + imgSize);
        r.width = w;
        r.height = h;
        r.method = "tga";
        return r;
    }
    return r;
}

TextureExtractResult tryRawMarker(const std::vector<uint8_t> &data)
{
    TextureExtractResult r;
    const int idx = findBytes(data, kTexDimMarker, sizeof(kTexDimMarker));
    if (idx < 0)
        return r;

    size_t imgOff = size_t(idx) + sizeof(kTexDimMarker);
    while (imgOff < data.size() && data[imgOff] == 0)
        ++imgOff;

    std::vector<uint8_t> block;
    if (tryBlock(data, 128, 128, imgOff, block)) {
        r.pixels = std::move(block);
        r.width = 128;
        r.height = 128;
        r.method = "raw_marker";
        return r;
    }

    const int tgaRef = findBytes(data, reinterpret_cast<const uint8_t *>(" .tga"), 4);
    if (tgaRef >= 0) {
        const size_t scanStart = size_t(tgaRef) + 4;
        const size_t scanEnd = std::min(data.size(), scanStart + 256);
        for (size_t off = scanStart; off + 2 <= scanEnd; ++off) {
            if (data[off] != 8 || data[off + 1] != 8)
                continue;
            for (auto [w, h] : {std::pair{128, 128}, {256, 128}, {128, 64}, {256, 64}, {64, 64}}) {
                size_t img = off + 2;
                while (img < data.size() && data[img] == 0)
                    ++img;
                if (tryBlock(data, w, h, img, block)) {
                    r.pixels = std::move(block);
                    r.width = w;
                    r.height = h;
                    r.method = "raw_marker";
                    return r;
                }
            }
        }
    }
    return r;
}

} // namespace

bool textureInvertOverrideForHash(const std::string &rcoMd5, bool &forceInvert, bool &skipInvert)
{
    forceInvert = false;
    skipInvert = false;
    std::ifstream in(invertConfigPath());
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find(rcoMd5) == std::string::npos)
            continue;
        if (line.find("invert") != std::string::npos)
            forceInvert = true;
        if (line.find("no-invert") != std::string::npos || line.find("skip") != std::string::npos)
            skipInvert = true;
        return true;
    }
    return false;
}

std::vector<uint8_t> proceduralWaveTexture(int w, int h)
{
    std::vector<uint8_t> tex(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float u = float(x) / float(std::max(1, w - 1));
            const float v = float(y) / float(std::max(1, h - 1));
            const float wave = 0.5f + 0.5f * std::sin(u * 6.28f * 3.f) * std::cos(v * 6.28f);
            tex[size_t(y) * w + x] = uint8_t(std::clamp(wave * 200.f + 40.f, 0.f, 255.f));
        }
    }
    return tex;
}

TextureExtractResult decodeGimFromBytes(const std::vector<uint8_t> &gimBytes)
{
    return tryGim(gimBytes);
}

TextureExtractResult extractTextureFromPrf(const std::vector<uint8_t> &data,
                                           const std::string &rcoMd5,
                                           const TextureExtractOptions &opts)
{
    TextureExtractOptions o = opts;
    bool forceInv = false, skipInv = false;
    if (textureInvertOverrideForHash(rcoMd5, forceInv, skipInv)) {
        o.forceInvert = o.forceInvert || forceInv;
        o.skipInvert = o.skipInvert || skipInv;
    }

    TextureExtractResult r;
    for (const auto &tryFn : {tryGim, tryTga, tryRawMarker}) {
        r = tryFn(data);
        if (!r.pixels.empty())
            break;
    }

    if (r.pixels.empty()) {
        r.pixels = proceduralWaveTexture();
        r.width = 128;
        r.height = 128;
        r.method = "procedural";
        return r;
    }

    r.pixels = normalizeBlock(std::move(r.pixels), o.allowInvert, o.forceInvert, o.skipInvert);
    r.inverted = o.forceInvert;
    return r;
}

} // namespace xmb

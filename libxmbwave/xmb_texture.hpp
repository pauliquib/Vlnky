// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace xmb {

struct TextureExtractResult {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    std::string method; // gim, tga, raw_marker, procedural
    bool inverted = false;
};

struct TextureExtractOptions {
    bool allowInvert = true;
    bool forceInvert = false;
    bool skipInvert = false;
};

/// Extract 8-bit grayscale wave reflection map from PRF/RCO blob.
TextureExtractResult extractTextureFromPrf(const std::vector<uint8_t> &data,
                                           const std::string &rcoMd5,
                                           const TextureExtractOptions &opts = {});

/// Decode GIM bytes (from RCO image tree) to grayscale pixels.
TextureExtractResult decodeGimFromBytes(const std::vector<uint8_t> &gimBytes);

std::vector<uint8_t> proceduralWaveTexture(int w = 128, int h = 128);

bool textureInvertOverrideForHash(const std::string &rcoMd5, bool &forceInvert, bool &skipInvert);

} // namespace xmb

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "rco_error.hpp"
#include "rco/rco_reader.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace xmb {

struct ChannelMap;

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Vec2 {
    float u = 0, v = 0;
};

enum class GridDimensionSource {
    Unknown,
    GmoMetadata,
    Inferred,
};

enum class IndexTopology {
    GridFallback,
    List,
    Strip,
};

enum class VertexOrder {
    FollowIndices,
    RowMajor,
    ColumnMajor,
};

struct GmoMesh {
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    int gridCols = 0;
    int gridRows = 0;
    std::vector<uint32_t> indices;
    GridDimensionSource gridSource = GridDimensionSource::Unknown;
    IndexTopology indexTopology = IndexTopology::GridFallback;
    bool windingCw = false;
    int faceCount = 0;
    bool usesGmoIndices = false;
    bool usesGmoUvs = false;
    int controlPointCount = 0;

    std::vector<float> controlPointsFlat() const;
    std::vector<uint8_t> toCacheBin() const;
};

struct FCurveKeyframe {
    float time = 0;
    float value = 0;
    float inTangent = 0;
    float outTangent = 0;
};

struct FCurveTrack {
    std::string name;
    int channel = 0;
    float duration = 1501.f;
    std::vector<FCurveKeyframe> keys;
    float evaluate(float t) const;
};

struct FCurveSet {
    std::vector<FCurveTrack> tracks;
    float loopDuration = 1501.f;
    float evaluateChannel(int channel, float t) const;
    std::string toAnimJson() const;
};

struct PrfFile {
    std::string path;
    std::vector<uint8_t> data;
    uint32_t version = 0;
    int gmoOffset = -1;
    std::vector<uint8_t> texture;
    int textureWidth = 0;
    int textureHeight = 0;
    std::string gmoHash;
    std::string md5() const;
};

using PrfResult = RcoResult<PrfFile>;
using GmoResult = RcoResult<GmoMesh>;
using FCurveResult = RcoResult<FCurveSet>;

PrfResult loadRcoResult(const std::string &path);

RcoLoadReport loadRcoBundle(const std::string &path,
                            PrfFile &prf,
                            GmoMesh &mesh,
                            FCurveSet &anim,
                            ChannelMap *outChannelMap = nullptr,
                            rco::WaveSceneTransform *outScene = nullptr,
                            bool *outSceneFound = nullptr);

/// Parser revision — bump to invalidate xmb_cache_v2 entries.
constexpr uint32_t kXmbParserVersion = 4u;

void ensurePlaceholderTexture(std::vector<uint8_t> &texture, int &texW, int &texH);

// Legacy throw wrappers (CLI)
PrfFile loadRco(const std::string &path);
GmoMesh parseGmo(const std::vector<uint8_t> &data, int gmoOffset);
FCurveSet parseFcurves(const std::vector<uint8_t> &data);
std::string writeCache(const PrfFile &prf, const GmoMesh &mesh, const FCurveSet &anim);

} // namespace xmb

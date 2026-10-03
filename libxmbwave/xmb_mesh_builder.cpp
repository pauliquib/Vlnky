// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_mesh_builder.hpp"

#include "xmb_channel_map.hpp"

#include <algorithm>
#include <cmath>

namespace xmb {
namespace {

float evalBinding(const FCurveSet *anim, const ChannelBinding &b, float timeMs)
{
    if (!anim || b.channel < 0)
        return 0.f;
    float v = anim->evaluateChannel(b.channel, timeMs);
    if (b.useAbs)
        v = std::abs(v);
    return v * b.scale;
}

Vec3 catmullRom(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2, const Vec3 &p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return Vec3 {
        0.5f * ((2.f * p1.x) + (-p0.x + p2.x) * t + (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x) * t2
                + (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x) * t3),
        0.5f * ((2.f * p1.y) + (-p0.y + p2.y) * t + (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y) * t2
                + (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y) * t3),
        0.5f * ((2.f * p1.z) + (-p0.z + p2.z) * t + (2.f * p0.z - 5.f * p1.z + 4.f * p2.z - p3.z) * t2
                + (-p0.z + 3.f * p1.z - 3.f * p2.z + p3.z) * t3),
    };
}

Vec3 gridAt(const std::vector<Vec3> &cp, int cols, int rows, int c, int r)
{
    c = std::max(0, std::min(c, cols - 1));
    r = std::max(0, std::min(r, rows - 1));
    const size_t i = size_t(r * cols + c);
    return i < cp.size() ? cp[i] : Vec3 {};
}

Vec3 sampleSurface(const std::vector<Vec3> &cp, int cols, int rows, float u, float v)
{
    const float uMax = float(std::max(1, cols - 1));
    const float vMax = float(std::max(1, rows - 1));
    u = std::max(0.f, std::min(u, uMax));
    v = std::max(0.f, std::min(v, vMax));

    const int cu = int(std::floor(u));
    const int cv = int(std::floor(v));
    const float fu = u - float(cu);
    const float fv = v - float(cv);

    Vec3 row[4];
    for (int j = 0; j < 4; ++j) {
        const int r = cv + j - 1;
        row[j] = catmullRom(gridAt(cp, cols, rows, cu - 1, r),
                            gridAt(cp, cols, rows, cu, r),
                            gridAt(cp, cols, rows, cu + 1, r),
                            gridAt(cp, cols, rows, cu + 2, r),
                            fu);
    }
    return catmullRom(row[0], row[1], row[2], row[3], fv);
}

int subdivisionsForLevel(int tessLevel)
{
    tessLevel = std::max(1, std::min(tessLevel, 3));
    switch (tessLevel) {
    case 1:
        return 4;
    case 2:
        return 8;
    default:
        return 16;
    }
}

void applyChannelDeform(std::vector<Vec3> &cp, int cols, int rows, const FCurveSet *anim, float timeMs, const ChannelMap &map)
{
    const bool hasAnim = anim && !anim->tracks.empty();
    if (!hasAnim)
        return;

    const float chY = evalBinding(anim, map.yOffset, timeMs);
    const float chY2 = evalBinding(anim, map.yWave, timeMs);
    const float chZ = evalBinding(anim, map.zOffset, timeMs);
    const float chPhase = evalBinding(anim, map.wavePhase, timeMs);
    const float chAmp = evalBinding(anim, map.waveAmplitude, timeMs);

    for (int r = 0; r < rows; ++r) {
        const float rowT = rows > 1 ? float(r) / float(rows - 1) : 0.f;
        const float rowWave = std::sin(rowT * 6.2831853f + chPhase);
        for (int c = 0; c < cols; ++c) {
            const size_t i = size_t(r * cols + c);
            if (i >= cp.size())
                break;
            auto &p = cp[i];
            p.y += chY + chY2;
            p.z += chZ + rowWave * (8.f + chAmp);
            p.y += rowWave * 3.f;
        }
    }
}

TessellatedMesh buildFromGmoIndices(const GmoMesh &controlMesh, const std::vector<Vec3> &cp)
{
    TessellatedMesh out;
    if (controlMesh.indices.empty())
        return out;

    out.vertices.reserve(controlMesh.positions.size() * 5);
    for (size_t i = 0; i < cp.size(); ++i) {
        const Vec2 uv = (i < controlMesh.uvs.size()) ? controlMesh.uvs[i] : Vec2 {};
        out.vertices.push_back(cp[i].x);
        out.vertices.push_back(cp[i].y);
        out.vertices.push_back(cp[i].z);
        out.vertices.push_back(uv.u);
        out.vertices.push_back(uv.v);
    }
    out.indices = controlMesh.indices;
    out.tessCols = controlMesh.gridCols;
    out.tessRows = controlMesh.gridRows;
    return out;
}

} // namespace

TessellatedMesh buildWaveMesh(const GmoMesh &controlMesh,
                              const FCurveSet *anim,
                              float timeMs,
                              int tessLevel,
                              const ChannelMap *channelMap)
{
    ChannelMap map = channelMap ? *channelMap : ChannelMap::defaultMap();

    TessellatedMesh out;
    const int cols = controlMesh.gridCols;
    const int rows = controlMesh.gridRows;
    if (cols < 2 || rows < 2 || controlMesh.positions.size() < size_t(cols * rows))
        return out;

    std::vector<Vec3> cp = controlMesh.positions;
    applyChannelDeform(cp, cols, rows, anim, timeMs, map);

    if (controlMesh.indexTopology != IndexTopology::GridFallback && controlMesh.indices.size() >= 3) {
        out = buildFromGmoIndices(controlMesh, cp);
        if (!out.vertices.empty())
            return out;
    }

    const int sub = subdivisionsForLevel(tessLevel);
    const int tessCols = (cols - 1) * sub + 1;
    const int tessRows = (rows - 1) * sub + 1;
    out.tessCols = tessCols;
    out.tessRows = tessRows;

    out.vertices.reserve(size_t(tessCols * tessRows * 5));
    for (int tr = 0; tr < tessRows; ++tr) {
        const float v = float(tr) / float(tessRows - 1) * float(rows - 1);
        for (int tc = 0; tc < tessCols; ++tc) {
            const float u = float(tc) / float(tessCols - 1) * float(cols - 1);
            const Vec3 p = sampleSurface(cp, cols, rows, u, v);
            const float uvU = float(tc) / float(std::max(1, tessCols - 1));
            const float uvV = float(tr) / float(std::max(1, tessRows - 1));
            out.vertices.push_back(p.x);
            out.vertices.push_back(p.y);
            out.vertices.push_back(p.z);
            out.vertices.push_back(uvU);
            out.vertices.push_back(uvV);
        }
    }

    out.indices.reserve(size_t((tessRows - 1) * (tessCols - 1) * 6));
    for (int r = 0; r < tessRows - 1; ++r) {
        for (int c = 0; c < tessCols - 1; ++c) {
            const uint32_t i0 = uint32_t(r * tessCols + c);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + uint32_t(tessCols);
            const uint32_t i3 = i2 + 1;
            out.indices.push_back(i0);
            out.indices.push_back(i2);
            out.indices.push_back(i1);
            out.indices.push_back(i1);
            out.indices.push_back(i2);
            out.indices.push_back(i3);
        }
    }

    return out;
}

std::vector<Vec3> buildDeformedControlPoints(const GmoMesh &controlMesh,
                                             const FCurveSet *anim,
                                             float timeMs,
                                             const ChannelMap *channelMap)
{
    ChannelMap map = channelMap ? *channelMap : ChannelMap::defaultMap();
    const int cols = controlMesh.gridCols;
    const int rows = controlMesh.gridRows;
    if (cols < 2 || rows < 2 || controlMesh.positions.size() < size_t(cols * rows))
        return {};
    std::vector<Vec3> cp = controlMesh.positions;
    applyChannelDeform(cp, cols, rows, anim, timeMs, map);
    return cp;
}

std::vector<uint32_t> buildDirectionalWireframeIndices(int tessCols, int tessRows, WireframeDirection dir)
{
    std::vector<uint32_t> lines;
    if (tessCols < 2 || tessRows < 2)
        return lines;

    if (dir == WireframeDirection::UOnly || dir == WireframeDirection::All) {
        for (int r = 0; r < tessRows; ++r) {
            for (int c = 0; c < tessCols - 1; ++c) {
                const uint32_t i0 = uint32_t(r * tessCols + c);
                lines.push_back(i0);
                lines.push_back(i0 + 1);
            }
        }
    }
    if (dir == WireframeDirection::VOnly || dir == WireframeDirection::All) {
        for (int c = 0; c < tessCols; ++c) {
            for (int r = 0; r < tessRows - 1; ++r) {
                const uint32_t i0 = uint32_t(r * tessCols + c);
                lines.push_back(i0);
                lines.push_back(i0 + uint32_t(tessCols));
            }
        }
    }
    return lines;
}

std::vector<uint32_t> buildGridWireframeIndices(int tessCols, int tessRows)
{
    return buildDirectionalWireframeIndices(tessCols, tessRows, WireframeDirection::All);
}

float countUpwardNormals(const std::vector<float> &vertices, const std::vector<uint32_t> &indices)
{
    if (indices.size() < 3)
        return 0.f;
    int up = 0;
    int total = 0;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const uint32_t i0 = indices[i];
        const uint32_t i1 = indices[i + 1];
        const uint32_t i2 = indices[i + 2];
        if (i0 * 5 + 4 >= vertices.size() || i1 * 5 + 4 >= vertices.size() || i2 * 5 + 4 >= vertices.size())
            continue;
        const float ax = vertices[i1 * 5] - vertices[i0 * 5];
        const float ay = vertices[i1 * 5 + 1] - vertices[i0 * 5 + 1];
        const float bx = vertices[i2 * 5] - vertices[i0 * 5];
        const float by = vertices[i2 * 5 + 1] - vertices[i0 * 5 + 1];
        const float crossZ = ax * by - ay * bx;
        if (crossZ > 0)
            ++up;
        ++total;
    }
    return total > 0 ? float(up) / float(total) : 0.f;
}

} // namespace xmb

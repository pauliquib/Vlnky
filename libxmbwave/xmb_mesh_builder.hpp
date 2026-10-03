// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "xmb_channel_map.hpp"
#include "xmbwave.hpp"

namespace xmb {

struct TessellatedMesh {
    std::vector<float> vertices; // x,y,z,u,v interleaved
    std::vector<uint32_t> indices;
    int tessCols = 0;
    int tessRows = 0;
};

/// Apply PSP fcurve offsets to control-point grid, then Catmull-Rom surface tessellation.
TessellatedMesh buildWaveMesh(const GmoMesh &controlMesh,
                              const FCurveSet *anim,
                              float timeMs,
                              int tessLevel,
                              const ChannelMap *channelMap = nullptr);

/// Deformed GMO control grid (before Catmull-Rom tessellation).
std::vector<Vec3> buildDeformedControlPoints(const GmoMesh &controlMesh,
                                             const FCurveSet *anim,
                                             float timeMs,
                                             const ChannelMap *channelMap = nullptr);

/// Line indices for tessellated grid wireframe (pairs of vertex indices).
enum class WireframeDirection {
    All,
    UOnly,
    VOnly,
};

std::vector<uint32_t> buildGridWireframeIndices(int tessCols, int tessRows);
std::vector<uint32_t> buildDirectionalWireframeIndices(int tessCols, int tessRows, WireframeDirection dir);

/// Fraction of triangle normals with positive Y (0..1).
float countUpwardNormals(const std::vector<float> &vertices, const std::vector<uint32_t> &indices);

} // namespace xmb

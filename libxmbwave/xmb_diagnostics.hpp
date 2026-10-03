// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "xmb_channel_map.hpp"
#include "xmbwave.hpp"

#include <string>

namespace xmb {

struct MeshBounds {
    Vec3 minPt {0, 0, 0};
    Vec3 maxPt {0, 0, 0};
    bool valid = false;
};

struct RuntimeFrameStats {
    float fps = 0.f;
    bool meshRebuild = false;
    int skipFrames = 0;
};

struct DiagnosticSnapshot {
    std::string rcoFile;
    std::string loadStatus;
    int gmoPoints = 0;
    int gridCols = 0;
    int gridRows = 0;
    GridDimensionSource gridSource = GridDimensionSource::Unknown;
    MeshBounds bbox;
    int texW = 0;
    int texH = 0;
    std::string textureFormat;
    float textureAvg = 0.f;
    bool textureInverted = false;
    std::string fcurveTrackSummary;
    std::string activeChannels;
    float loopDurationMs = 0.f;
    int tessLevel = 0;
    int vertexCount = 0;
    int indexCount = 0;
    RuntimeFrameStats frame;
};

MeshBounds computeMeshBounds(const std::vector<float> &vertices);
float computeTextureAverage(const std::vector<uint8_t> &texture);
std::string gridSourceLabel(GridDimensionSource src);
std::string formatTextureMethod(const std::string &method);
std::string buildActiveChannelLine(const FCurveSet *anim, float timeMs, const ChannelMap &map);
std::string formatDiagnosticOverlay(const DiagnosticSnapshot &snap);

} // namespace xmb

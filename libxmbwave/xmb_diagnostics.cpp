// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_diagnostics.hpp"

#include "xmb_channel_map.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace xmb {
namespace {

float evalRawChannel(const FCurveSet *anim, int channel, float timeMs)
{
    if (!anim || channel < 0)
        return 0.f;
    return anim->evaluateChannel(channel, timeMs);
}

} // namespace

MeshBounds computeMeshBounds(const std::vector<float> &vertices)
{
    MeshBounds b;
    if (vertices.size() < 5)
        return b;

    b.minPt = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
    b.maxPt = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};

    for (size_t i = 0; i + 2 < vertices.size(); i += 5) {
        b.minPt.x = std::min(b.minPt.x, vertices[i]);
        b.minPt.y = std::min(b.minPt.y, vertices[i + 1]);
        b.minPt.z = std::min(b.minPt.z, vertices[i + 2]);
        b.maxPt.x = std::max(b.maxPt.x, vertices[i]);
        b.maxPt.y = std::max(b.maxPt.y, vertices[i + 1]);
        b.maxPt.z = std::max(b.maxPt.z, vertices[i + 2]);
    }
    b.valid = b.maxPt.x >= b.minPt.x;
    return b;
}

float computeTextureAverage(const std::vector<uint8_t> &texture)
{
    if (texture.empty())
        return 0.f;
    double sum = 0;
    for (uint8_t b : texture)
        sum += b;
    return float(sum / double(texture.size()));
}

std::string gridSourceLabel(GridDimensionSource src)
{
    switch (src) {
    case GridDimensionSource::GmoMetadata:
        return "metadata";
    case GridDimensionSource::Inferred:
        return "inferred";
    default:
        return "unknown";
    }
}

std::string formatTextureMethod(const std::string &method)
{
    if (method.empty())
        return "PROC";
    std::string upper = method;
    for (char &c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper.find("GIM") != std::string::npos)
        return "GIM";
    if (upper.find("TGA") != std::string::npos)
        return "TGA";
    if (upper.find("RAW") != std::string::npos)
        return "RAW";
    if (upper.find("PLACEHOLDER") != std::string::npos || upper.find("PROC") != std::string::npos)
        return "PROC";
    return upper;
}

std::string buildActiveChannelLine(const FCurveSet *anim, float timeMs, const ChannelMap &map)
{
    std::ostringstream oss;
    oss << "ch" << map.yOffset.channel << "=" << evalRawChannel(anim, map.yOffset.channel, timeMs);
    oss << " ch" << map.yWave.channel << "=" << evalRawChannel(anim, map.yWave.channel, timeMs);
    oss << " ch" << map.zOffset.channel << "=" << evalRawChannel(anim, map.zOffset.channel, timeMs);
    oss << " ch" << map.wavePhase.channel << "=" << evalRawChannel(anim, map.wavePhase.channel, timeMs);
    return oss.str();
}

std::string formatDiagnosticOverlay(const DiagnosticSnapshot &snap)
{
    std::ostringstream oss;
    oss << "RCO: " << snap.rcoFile << '\n';
    oss << "LoadStatus: " << snap.loadStatus << '\n';
    oss << "GMO: points=" << snap.gmoPoints << " grid=" << snap.gridCols << 'x' << snap.gridRows
        << " source=" << gridSourceLabel(snap.gridSource) << '\n';

    if (snap.bbox.valid) {
        oss << "BBox: min(" << snap.bbox.minPt.x << ',' << snap.bbox.minPt.y << ',' << snap.bbox.minPt.z
            << ") max(" << snap.bbox.maxPt.x << ',' << snap.bbox.maxPt.y << ',' << snap.bbox.maxPt.z << ")\n";
    } else {
        oss << "BBox: (empty)\n";
    }

    oss << "Texture: " << snap.texW << 'x' << snap.texH << " format=" << snap.textureFormat
        << " avg=" << snap.textureAvg << " inverted=" << (snap.textureInverted ? 'Y' : 'N') << '\n';
    oss << "fcurve tracks: " << (snap.fcurveTrackSummary.empty() ? "(none)" : snap.fcurveTrackSummary) << '\n';
    oss << "Active channels: " << snap.activeChannels << '\n';
    oss << "Loop duration: " << snap.loopDurationMs << "ms\n";
    oss << "TessLevel: " << snap.tessLevel << " vertices=" << snap.vertexCount
        << " indices=" << snap.indexCount << '\n';
    oss << "Frame: " << snap.frame.fps << " rebuild=" << (snap.frame.meshRebuild ? 'Y' : 'N')
        << " skip=" << snap.frame.skipFrames;
    return oss.str();
}

} // namespace xmb

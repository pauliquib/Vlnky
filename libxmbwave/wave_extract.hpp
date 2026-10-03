// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "rco/rco_reader.hpp"
#include "xmb_channel_map.hpp"
#include "xmbwave.hpp"

namespace xmb {

struct WaveExtractReport {
    RcoLoadReport load;
    std::string extractionMethod = "tree";
    std::string modelLabel;
    std::string imageLabel;
    rco::WaveSceneTransform sceneTransform;
    bool sceneFound = false;
};

struct WaveExtractResult {
    WaveExtractReport report;
    PrfFile prf;
    GmoMesh mesh;
    FCurveSet anim;
    ChannelMap channelMap;
};

WaveExtractResult extractWaveFromRcoData(const std::vector<uint8_t> &data, const std::string &pathHint = {});

ChannelMap channelMapFromFcurves(const FCurveSet &anim);

} // namespace xmb

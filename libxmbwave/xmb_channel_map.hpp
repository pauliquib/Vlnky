// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "xmbwave.hpp"

#include <string>

namespace xmb {

/// Maps fcurve channel IDs to mesh deformation (configurable via JSON).
struct ChannelBinding {
    int channel = -1;
    float scale = 1.f;
    bool useAbs = false;
};

struct ChannelMap {
    ChannelBinding yOffset;
    ChannelBinding yWave;
    ChannelBinding zOffset;
    ChannelBinding wavePhase;
    ChannelBinding waveAmplitude;

    static ChannelMap defaultMap();
    /// Load ~/.config/vlnky/channel_map.json; optional per-rco hash override.
    static ChannelMap loadForRco(const std::string &rcoPath, const std::string &rcoMd5);
};

/// Human-readable summary of present channels and value ranges (for diagnostics).
std::string fcurveChannelSummary(const FCurveSet &anim);

} // namespace xmb

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "rco_error.hpp"
#include "xmbwave.hpp"

namespace xmb {

struct GmoParseInfo {
    GridDimensionSource gridSource = GridDimensionSource::Unknown;
    bool usedGmoIndices = false;
    bool usedGmoUvs = false;
    int controlPointCount = 0;
    std::string log;
};

struct GmoParseOptions {
    float positionBound = 80.f;
    size_t minControlPoints = 50;
    VertexOrder vertexOrder = VertexOrder::FollowIndices;
};

GmoResult parseGmoResult(const std::vector<uint8_t> &data, int gmoOffset, GmoParseInfo *info = nullptr);
GmoResult parseGmoResult(const std::vector<uint8_t> &data, int gmoOffset, const GmoParseOptions &opts, GmoParseInfo *info);

} // namespace xmb

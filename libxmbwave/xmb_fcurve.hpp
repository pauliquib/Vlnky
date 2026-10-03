// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "rco_error.hpp"
#include "xmbwave.hpp"

namespace xmb {

FCurveResult parseFcurvesResult(const std::vector<uint8_t> &data);

/// Effective loop duration: max track duration; logs warning if tracks disagree (via out param).
float computeLoopDuration(const FCurveSet &anim, bool *multipleDurations = nullptr);

} // namespace xmb

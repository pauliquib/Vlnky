// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../libxmbwave/rco/rco_reader.hpp"
#include "../libxmbwave/wave_extract.hpp"
#include "../libxmbwave/xmbwave.hpp"

#include <QString>
#include <QRunnable>
#include <atomic>
#include <memory>

struct RcoLoadPayload {
    QString rcoPath;
    int generation = 0;
};

struct RcoLoadOutcome {
    int generation = 0;
    xmb::RcoLoadReport report;
    xmb::PrfFile prf;
    xmb::GmoMesh mesh;
    xmb::FCurveSet anim;
    xmb::ChannelMap channelMap;
    xmb::rco::WaveSceneTransform sceneTransform;
    bool sceneFound = false;
    bool success = false;
};

class RcoLoadTask : public QRunnable
{
public:
    explicit RcoLoadTask(RcoLoadPayload payload);

    void run() override;

    RcoLoadOutcome outcome;

private:
    RcoLoadPayload m_payload;
};

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rco_load_task.h"

RcoLoadTask::RcoLoadTask(RcoLoadPayload payload)
    : m_payload(std::move(payload))
{
    setAutoDelete(false);
}

void RcoLoadTask::run()
{
    outcome.generation = m_payload.generation;

    const auto path = m_payload.rcoPath.toStdString();
    outcome.report = xmb::loadRcoBundle(path,
                                        outcome.prf,
                                        outcome.mesh,
                                        outcome.anim,
                                        &outcome.channelMap,
                                        &outcome.sceneTransform,
                                        &outcome.sceneFound);
    outcome.success = outcome.report.hasMesh;
    if (!outcome.success && outcome.report.error == xmb::RcoError::Ok)
        outcome.report.error = xmb::RcoError::TooFewControlPoints;
}

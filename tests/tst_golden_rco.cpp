// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmbwave.hpp"

#include <QtTest>
#include <filesystem>
#include <string>

class TstGoldenRco : public QObject
{
    Q_OBJECT

private slots:
    void parseGoldenWaves();
};

void TstGoldenRco::parseGoldenWaves()
{
    const std::filesystem::path root(VLNKY_WAVE_COLLECTION);
    const char *names[] = {"Alice", "aquadark", "Blank", "1up", "Blue Wire"};
    for (const char *name : names) {
        const auto path = (root / name / "system_plugin_bg.rco").string();
        if (!std::filesystem::exists(path))
            QSKIP("wave missing");

        xmb::PrfFile prf;
        xmb::GmoMesh mesh;
        xmb::FCurveSet anim;
        const auto report = xmb::loadRcoBundle(path, prf, mesh, anim);
        QVERIFY2(report.hasMesh, name);
        QVERIFY(mesh.positions.size() > 50);
    }
}

QTEST_MAIN(TstGoldenRco)
#include "tst_golden_rco.moc"

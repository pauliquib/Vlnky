// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmb_gmo_parser.hpp"
#include "../libxmbwave/xmb_mesh_builder.hpp"
#include "../libxmbwave/xmbwave.hpp"

#include <QtTest>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

class TstGmoOrientation : public QObject
{
    Q_OBJECT

private slots:
    void gridColsGreaterThanRows();
    void bboxWideOnX();
    void upwardNormalsAtRest();
};

void TstGmoOrientation::gridColsGreaterThanRows()
{
    const std::filesystem::path root(VLNKY_WAVE_COLLECTION);
    const char *names[] = {"Blue Wire", "Alice", "aquadark"};
    for (const char *name : names) {
        const auto path = root / name / "system_plugin_bg.rco";
        if (!std::filesystem::exists(path))
            QSKIP("wave missing");

        xmb::PrfFile prf;
        xmb::GmoMesh mesh;
        xmb::FCurveSet anim;
        const auto report = xmb::loadRcoBundle(path.string(), prf, mesh, anim);
        QVERIFY2(report.hasMesh, name);
        QVERIFY2(mesh.gridCols > mesh.gridRows, name);
    }
}

void TstGmoOrientation::bboxWideOnX()
{
    const std::filesystem::path root(VLNKY_WAVE_COLLECTION);
    const char *names[] = {"Blue Wire", "Alice", "aquadark"};
    for (const char *name : names) {
        const auto path = root / name / "system_plugin_bg.rco";
        if (!std::filesystem::exists(path))
            QSKIP("wave missing");

        xmb::PrfFile prf;
        xmb::GmoMesh mesh;
        xmb::FCurveSet anim;
        const auto report = xmb::loadRcoBundle(path.string(), prf, mesh, anim);
        QVERIFY2(report.hasMesh, name);
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (const auto &p : mesh.positions) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        const float spanX = maxX - minX;
        const float spanY = maxY - minY;
        QVERIFY2(spanX > spanY, qPrintable(QString("%1 spanX=%2 spanY=%3").arg(name).arg(spanX).arg(spanY)));
    }
}

void TstGmoOrientation::upwardNormalsAtRest()
{
    const std::filesystem::path root(VLNKY_WAVE_COLLECTION);
    const char *names[] = {"Blue Wire", "Alice", "aquadark"};
    for (const char *name : names) {
        const auto path = root / name / "system_plugin_bg.rco";
        if (!std::filesystem::exists(path))
            QSKIP("wave missing");

        xmb::PrfFile prf;
        xmb::GmoMesh mesh;
        xmb::FCurveSet anim;
        const auto report = xmb::loadRcoBundle(path.string(), prf, mesh, anim);
        QVERIFY2(report.hasMesh, name);
        const auto tess = xmb::buildWaveMesh(mesh, nullptr, 0.f, 2);
        QVERIFY(!tess.vertices.empty());
        const float upRatio = xmb::countUpwardNormals(tess.vertices, tess.indices);
        QVERIFY2(upRatio > 0.5f, qPrintable(QString("%1 upRatio=%2").arg(name).arg(upRatio)));
    }
}

QTEST_MAIN(TstGmoOrientation)
#include "tst_gmo_orientation.moc"

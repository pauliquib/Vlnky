// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmb_mesh_builder.hpp"

#include <QtTest>

class TstNoFakeAnim : public QObject
{
    Q_OBJECT

private slots:
    void staticMeshWithoutFcurve();
};

void TstNoFakeAnim::staticMeshWithoutFcurve()
{
    xmb::GmoMesh mesh;
    mesh.gridCols = 4;
    mesh.gridRows = 3;
    for (int r = 0; r < mesh.gridRows; ++r) {
        for (int c = 0; c < mesh.gridCols; ++c) {
            mesh.positions.push_back({float(c), float(r), float(c + r)});
            mesh.uvs.push_back({float(c) / 3.f, float(r) / 2.f});
        }
    }

    const auto at0 = xmb::buildDeformedControlPoints(mesh, nullptr, 0.f);
    const auto at100 = xmb::buildDeformedControlPoints(mesh, nullptr, 100.f);
    QCOMPARE(at0.size(), at100.size());
    for (size_t i = 0; i < at0.size(); ++i) {
        QCOMPARE(at0[i].x, at100[i].x);
        QCOMPARE(at0[i].y, at100[i].y);
        QCOMPARE(at0[i].z, at100[i].z);
    }
}

QTEST_MAIN(TstNoFakeAnim)
#include "tst_no_fake_anim.moc"

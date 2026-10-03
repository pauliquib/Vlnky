// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmbwave.hpp"

#include <QtTest>
#include <cmath>

class TstFcurveHermite : public QObject
{
    Q_OBJECT

private slots:
    void goldenInterpolation();
    void singleKey();
};

void TstFcurveHermite::goldenInterpolation()
{
    xmb::FCurveTrack tr;
    tr.duration = 100.f;
    tr.keys = {
        {0.f, 0.f, 0.f, 0.f},
        {50.f, 10.f, 0.f, 0.f},
        {100.f, 0.f, 0.f, 0.f},
    };

    const float mid = tr.evaluate(25.f);
    QVERIFY(mid > 0.f && mid < 10.f);

    const float at50 = tr.evaluate(50.f);
    QCOMPARE(at50, 10.f);
}

void TstFcurveHermite::singleKey()
{
    xmb::FCurveTrack tr;
    tr.keys.push_back({0.f, 3.5f, 0.f, 0.f});
    QCOMPARE(tr.evaluate(42.f), 3.5f);
}

QTEST_MAIN(TstFcurveHermite)
#include "tst_fcurve_hermite.moc"

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "psp_wave.hpp"

#include <QDir>
#include <QTest>

#include <cmath>

class TstPspWave : public QObject
{
    Q_OBJECT

private slots:
    void wholeCollectionLoads()
    {
        QDir dir(QStringLiteral(VLNKY_WAVE_COLLECTION));
        if (!dir.exists())
            QSKIP("wave collection not found");
        int loaded = 0;
        for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString rco = dir.filePath(name + QStringLiteral("/system_plugin_bg.rco"));
            if (!QFileInfo::exists(rco))
                continue;
            xmb::PspWave wave;
            const auto r = xmb::loadPspWaveRco(rco.toStdString(), wave);
            QVERIFY2(r.ok, qPrintable(name + QStringLiteral(": ") + QString::fromStdString(r.error)));
            QVERIFY2(wave.uCount >= 4 && wave.vCount >= 4, qPrintable(name));
            QVERIFY2(wave.morphCount >= 1, qPrintable(name));
            QVERIFY2(wave.texW > 0 && wave.texH > 0, qPrintable(name + QStringLiteral(" has no texture")));
            QVERIFY2(wave.matrixTrack.valid() && wave.morphTrack.valid(), qPrintable(name + QStringLiteral(" not animated")));
            QVERIFY2(wave.loopFrames() > 0.f, qPrintable(name));

            xmb::PspWaveTessellator tess;
            tess.setup(wave, 64, 24);
            std::vector<float> a, b;
            tess.evaluate(wave, wave.frameStart, a);
            tess.evaluate(wave, wave.frameStart + wave.loopFrames() * 0.3f, b);
            QCOMPARE(a.size(), size_t(tess.uSamples() * tess.vSamples() * 6));
            float diff = 0.f;
            for (size_t i = 0; i < a.size(); ++i) {
                QVERIFY2(std::isfinite(a[i]), qPrintable(name));
                diff = std::max(diff, std::abs(a[i] - b[i]));
            }
            QVERIFY2(diff > 1.f, qPrintable(name + QStringLiteral(" does not move")));
            ++loaded;
        }
        QVERIFY(loaded > 0);
        qInfo("loaded %d waves", loaded);
    }

    void trackInterpolatesLinearly()
    {
        xmb::PspWaveTrack t;
        t.dims = 1;
        t.times = {0.f, 10.f};
        t.values = {2.f, 4.f};
        float v = 0.f;
        t.evaluate(5.f, &v);
        QCOMPARE(v, 3.f);
        t.evaluate(20.f, &v);
        QCOMPARE(v, 4.f);
    }
};

QTEST_GUILESS_MAIN(TstPspWave)
#include "tst_psp_wave.moc"

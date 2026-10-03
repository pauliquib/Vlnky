// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmb_gmo_parser.hpp"
#include "../libxmbwave/xmbwave.hpp"

#include <QtTest>
#include <QDir>
#include <filesystem>
#include <fstream>

class TstRcoBounds : public QObject
{
    Q_OBJECT

private slots:
    void truncatedPrfBuffer();
    void invalidMagic();
    void gmoOverrun();
};

void TstRcoBounds::truncatedPrfBuffer()
{
    std::vector<uint8_t> tiny = {0x00, 'P', 'R', 'F', 0x71, 0, 0, 0};
    auto r = xmb::parseGmoResult(tiny, -1);
    QVERIFY(!r);
    QCOMPARE(int(r.error), int(xmb::RcoError::GmoNotFound));
}

void TstRcoBounds::invalidMagic()
{
    const std::string path = QDir::temp().filePath(QStringLiteral("xmb_bad.rco")).toStdString();
    {
        std::ofstream out(path, std::ios::binary);
        out.write("BAD!", 4);
    }
    auto r = xmb::loadRcoResult(path);
    QVERIFY(!r);
    QCOMPARE(int(r.error), int(xmb::RcoError::InvalidPrfMagic));
    std::filesystem::remove(path);
}

void TstRcoBounds::gmoOverrun()
{
    std::vector<uint8_t> data(64, 0);
    data[0] = 0;
    data[1] = 'P';
    data[2] = 'R';
    data[3] = 'F';
    auto prf = xmb::loadRcoResult("/nonexistent/file.rco");
    QVERIFY(!prf);
    QCOMPARE(int(prf.error), int(xmb::RcoError::FileNotFound));
}

QTEST_MAIN(TstRcoBounds)
#include "tst_rco_bounds.moc"

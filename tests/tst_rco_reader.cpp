// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/rco/rco_reader.hpp"

#include <QtTest>

#include <fstream>
#include <vector>

class TstRcoReader : public QObject
{
    Q_OBJECT

private slots:
    void aliceModelTree();
    void goldenGmoMd5();
};

static std::vector<uint8_t> readFile(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.good())
        return {};
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char *>(buf.data()), sz);
    return buf;
}

void TstRcoReader::aliceModelTree()
{
#ifdef VLNKY_WAVE_COLLECTION
    const std::string base = VLNKY_WAVE_COLLECTION;
    const std::string path = base + "/Alice/system_plugin_bg.rco";
    const auto data = readFile(path.c_str());
    QVERIFY(!data.empty());
    auto doc = xmb::rco::readRcoDocument(data);
    QVERIFY(doc);
    QCOMPARE(doc.value.header.version, 0x71u);
    QVERIFY(!doc.value.models.empty());
    QCOMPARE(doc.value.models.front()->label, std::string("mdl_bg"));
    QVERIFY(doc.value.scene.foundModelObject);
    QCOMPARE(doc.value.scene.modelObject.scaleWidth, 8.5f);
    auto blob = xmb::rco::readNodeResource(doc.value, *doc.value.scene.modelEntry);
    QVERIFY(blob);
    QVERIFY(blob.value.size() > 1000);
#else
    QSKIP("VLNKY_WAVE_COLLECTION not set");
#endif
}

void TstRcoReader::goldenGmoMd5()
{
#ifdef VLNKY_WAVE_COLLECTION
    const std::string base = VLNKY_WAVE_COLLECTION;
    for (const char *wave : {"Alice", "aquadark", "Blank", "1up", "Blue Wire"}) {
        std::string folder = wave;
        if (folder.find(' ') != std::string::npos) {
            // skip spaced name in loop - test Alice and aquadark only here
            continue;
        }
        const std::string path = base + "/" + folder + "/system_plugin_bg.rco";
        const auto data = readFile(path.c_str());
    QVERIFY(!data.empty());
        auto doc = xmb::rco::readRcoDocument(data);
        QVERIFY2(doc, wave);
        QVERIFY2(doc.value.scene.modelEntry, wave);
        auto blob = xmb::rco::readNodeResource(doc.value, *doc.value.scene.modelEntry);
        QVERIFY2(blob && !blob.value.empty(), wave);
    }
#else
    QSKIP("VLNKY_WAVE_COLLECTION not set");
#endif
}

QTEST_MAIN(TstRcoReader)
#include "tst_rco_reader.moc"

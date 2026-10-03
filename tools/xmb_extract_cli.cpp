// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../libxmbwave/xmbwave.hpp"

#include <QCoreApplication>
#include <cstring>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::cerr << "usage: vlnky-extract <system_plugin_bg.rco>\n";
        return 1;
    }

    const QString rco = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    if (!QFileInfo::exists(rco)) {
        std::cerr << "error: not found: " << rco.toStdString() << '\n';
        return 2;
    }

    xmb::PrfFile prf;
    xmb::GmoMesh mesh;
    xmb::FCurveSet anim;
    const auto report = xmb::loadRcoBundle(rco.toStdString(), prf, mesh, anim);
    if (!report.hasMesh) {
        std::cerr << "error: " << xmb::rcoErrorString(report.error) << '\n';
        return 3;
    }

    const auto cacheDir = QString::fromStdString(xmb::writeCache(prf, mesh, anim));
    const QString png = QDir(cacheDir).filePath(QStringLiteral("texture.png"));

    if (!prf.texture.empty()) {
        QImage img(prf.textureWidth, prf.textureHeight, QImage::Format_Grayscale8);
        std::memcpy(img.bits(), prf.texture.data(),
                    std::min(prf.texture.size(), size_t(prf.textureWidth * prf.textureHeight)));
        img.convertToFormat(QImage::Format_RGBA8888).save(png);
    }

    std::cout << png.toStdString() << std::endl;
    return 0;
}

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_wave_renderer.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QImage>
#include <QTimer>

#include <iostream>

static const char kPreviewQml[] = R"QML(
import QtQuick
import org.psvec.vlnky

Window {
    id: win
    width: 960
    height: 540
    visible: true
    color: "#080c1c"

    property string rcoPath: ""
    property bool screenshotMode: false

    XmbWaveItem {
        id: wave
        objectName: "wave"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height
        rcoPath: win.rcoPath
        tessLevel: 2
        maxFps: 30
        debugOverlay: false
        pauseOnBattery: false

        Component.onCompleted: {
            if (rcoPath.length > 2)
                reload()
        }

        onLoadedChanged: {
            if (loaded && win.screenshotMode)
                captureTimer.start()
        }

        onAllScreenshotsCaptured: function(dir) {
            console.log("screenshots saved to", dir)
            Qt.quit()
        }
    }

    Timer {
        id: captureTimer
        interval: 900
        repeat: false
        onTriggered: wave.captureAllDebugScreenshots()
    }
}
)QML";

int main(int argc, char *argv[])
{
    qputenv("QSG_RHI_BACKEND", "opengl");
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("vlnky-preview"));
    app.setApplicationVersion(QStringLiteral("1.5.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Preview and screenshot capture for Vlnky waves"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption rcoOpt({QStringLiteral("r"), QStringLiteral("rco")},
                              QStringLiteral("Path to system_plugin_bg.rco"),
                              QStringLiteral("path"));
    QCommandLineOption screenshotOpt({QStringLiteral("s"), QStringLiteral("screenshot")},
                                     QStringLiteral("Capture debug screenshots to ~/.cache/vlnky/screenshots/"),
                                     QStringLiteral("dir"));
    QCommandLineOption grabOpt({QStringLiteral("g"), QStringLiteral("grab")},
                               QStringLiteral("Grab the window to <png> after --grab-delay ms and quit"),
                               QStringLiteral("png"));
    QCommandLineOption grabDelayOpt(QStringLiteral("grab-delay"), QStringLiteral("Delay before --grab (ms)"),
                                    QStringLiteral("ms"), QStringLiteral("2500"));
    QCommandLineOption debugModeOpt(QStringLiteral("mode"), QStringLiteral("Debug render mode 0-3"),
                                    QStringLiteral("mode"), QStringLiteral("0"));
    parser.addOption(rcoOpt);
    parser.addOption(screenshotOpt);
    parser.addOption(grabOpt);
    parser.addOption(grabDelayOpt);
    QCommandLineOption setOpt(QStringLiteral("set"),
                              QStringLiteral("Set an XmbWaveItem property, e.g. --set bloom=0.5 (repeatable)"),
                              QStringLiteral("prop=value"));
    QCommandLineOption sizeOpt(QStringLiteral("size"), QStringLiteral("Window size WxH"), QStringLiteral("WxH"),
                               QStringLiteral("960x540"));
    parser.addOption(debugModeOpt);
    parser.addOption(setOpt);
    parser.addOption(sizeOpt);
    parser.process(app);

    const QString rco = parser.value(rcoOpt);
    if (!rco.isEmpty() && !QFileInfo::exists(rco)) {
        std::cerr << "error: RCO not found: " << rco.toStdString() << '\n';
        return 2;
    }

    const bool screenshotMode = parser.isSet(screenshotOpt);

    qmlRegisterType<XmbWaveRhiItem>("org.psvec.vlnky", 1, 0, "XmbWaveItem");

    QQmlApplicationEngine engine;
    engine.loadData(QByteArray(kPreviewQml));

    if (engine.rootObjects().isEmpty()) {
        std::cerr << "error: failed to load preview QML\n";
        return 3;
    }

    QObject *root = engine.rootObjects().first();
    root->setProperty("rcoPath", rco);
    root->setProperty("screenshotMode", screenshotMode);

    const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
    if (wh.size() == 2) {
        root->setProperty("width", wh[0].toInt());
        root->setProperty("height", wh[1].toInt());
    }
    if (QObject *wave = root->findChild<QObject *>(QStringLiteral("wave"))) {
        wave->setProperty("debugRenderMode", parser.value(debugModeOpt).toInt());
        for (const QString &kv : parser.values(setOpt)) {
            const qsizetype eq = kv.indexOf(QLatin1Char('='));
            if (eq > 0)
                wave->setProperty(kv.left(eq).toUtf8().constData(), kv.mid(eq + 1));
        }
    }

    if (parser.isSet(grabOpt)) {
        const QString out = parser.value(grabOpt);
        QTimer::singleShot(parser.value(grabDelayOpt).toInt(), &app, [root, out]() {
            if (auto *win = qobject_cast<QQuickWindow *>(root)) {
                const QImage img = win->grabWindow();
                std::cout << (img.save(out) ? "grabbed " : "grab failed ") << out.toStdString() << std::endl;
            }
            QCoreApplication::quit();
        });
    } else if (!screenshotMode) {
        QTimer::singleShot(15000, &app, &QCoreApplication::quit);
    }

    return app.exec();
}

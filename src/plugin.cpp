// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>

#include "wave_scanner.h"
#include "xmb_wave_renderer.h"

class VlnkyPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<XmbWaveRhiItem>(uri, 1, 0, "XmbWaveItem");
        qmlRegisterType<WaveScanner>(uri, 1, 0, "WaveScanner");
    }
};

#include "plugin.moc"

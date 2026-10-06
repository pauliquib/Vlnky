// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class WaveScanner : public QObject
{
    Q_OBJECT
    // Registered as WaveScanner in plugin.cpp
    Q_PROPERTY(QString collectionPath READ collectionPath WRITE setCollectionPath NOTIFY collectionPathChanged)
    Q_PROPERTY(QStringList waves READ waves NOTIFY wavesChanged)
    Q_PROPERTY(QString selectedWave READ selectedWave WRITE setSelectedWave NOTIFY selectedWaveChanged)
    Q_PROPERTY(QString selectedRcoPath READ selectedRcoPath NOTIFY selectedRcoPathChanged)

public:
    explicit WaveScanner(QObject *parent = nullptr);

    QString collectionPath() const;
    void setCollectionPath(const QString &path);

    QStringList waves() const { return m_waves; }

    QString selectedWave() const;
    void setSelectedWave(const QString &name);

    QString selectedRcoPath() const;

    Q_INVOKABLE QString previewImageFor(const QString &waveName) const;
    Q_INVOKABLE static bool isValidCollection(const QString &path);
    Q_INVOKABLE static QString resolveDefaultCollection();
    Q_INVOKABLE static bool rcoFileExists(const QString &path);
    Q_INVOKABLE void refresh();

    // GPU preference. The wave is drawn by QQuickRhiItem inside plasmashell's QRhi
    // scene graph, so it cannot pick a GPU per item; the choice is applied to the
    // whole shell via a systemd user drop-in (VK_ICD_FILENAMES /
    // __EGL_VENDOR_LIBRARY_FILENAMES) and takes effect after a plasmashell restart.
    // pref: 0 = automatic (remove the drop-in), 1 = integrated, 2 = dedicated.
    Q_INVOKABLE static QString applyGpuPreference(int pref);
    Q_INVOKABLE static QStringList detectedGpus();
    Q_INVOKABLE static void restartPlasmashell();

signals:
    void collectionPathChanged();
    void wavesChanged();
    void selectedWaveChanged();
    void selectedRcoPathChanged();

private:
    bool loadCache(QStringList *waves) const;
    void saveCache(const QStringList &waves) const;

    QString m_collectionPath;
    QStringList m_waves;
    QString m_selectedWave;
};

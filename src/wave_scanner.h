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

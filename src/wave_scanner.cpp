// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wave_scanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

bool collectionHasWaves(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QDir dir(path.trimmed().remove(QStringLiteral("file://")));
    if (!dir.exists())
        return false;
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo(dir.filePath(entry + QStringLiteral("/system_plugin_bg.rco"))).isFile())
            return true;
    }
    return false;
}

QString cacheFilePath(const QString &collectionPath)
{
    return QDir(collectionPath).filePath(QStringLiteral(".psp_xmb_waves.json"));
}

qint64 collectionMtime(const QString &collectionPath)
{
    QFileInfo info(collectionPath);
    return info.isDir() ? info.lastModified().toSecsSinceEpoch() : 0;
}

} // namespace

WaveScanner::WaveScanner(QObject *parent)
    : QObject(parent)
{
}

QString WaveScanner::collectionPath() const
{
    return m_collectionPath;
}

void WaveScanner::setCollectionPath(const QString &path)
{
    const QString normalized = path.trimmed().remove(QStringLiteral("file://"));
    if (m_collectionPath == normalized)
        return;
    m_collectionPath = normalized;
    emit collectionPathChanged();
    refresh();
}

QString WaveScanner::selectedWave() const
{
    return m_selectedWave;
}

void WaveScanner::setSelectedWave(const QString &name)
{
    if (m_selectedWave == name)
        return;
    m_selectedWave = name;
    emit selectedWaveChanged();
    emit selectedRcoPathChanged();
}

QString WaveScanner::selectedRcoPath() const
{
    if (m_collectionPath.isEmpty() || m_selectedWave.isEmpty())
        return {};
    return QDir(m_collectionPath).filePath(m_selectedWave + QStringLiteral("/system_plugin_bg.rco"));
}

bool WaveScanner::isValidCollection(const QString &path)
{
    return collectionHasWaves(path);
}

QString WaveScanner::resolveDefaultCollection()
{
    QStringList candidates;
    const QByteArray env = qgetenv("VLNKY_WAVE_COLLECTION");
    if (!env.isEmpty())
        candidates.append(QString::fromUtf8(env));

    const QStringList configRoots = {
        QDir::homePath() + QStringLiteral("/.local/share/plasma/wallpapers/org.psvec.vlnky/contents/config"),
        QStringLiteral("/usr/share/plasma/wallpapers/org.psvec.vlnky/contents/config"),
    };
    for (const QString &root : configRoots) {
        QFile f(QDir(root).filePath(QStringLiteral("default_collection.path")));
        if (f.open(QIODevice::ReadOnly)) {
            const QString fromInstall = QString::fromUtf8(f.readAll()).trimmed();
            if (!fromInstall.isEmpty())
                candidates.append(fromInstall);
        }
    }

    const QString home = QDir::homePath();
    candidates.append(home + QStringLiteral("/XMB waves/176 XMB WAVES for 5.00/176 XMB WAVES"));
    candidates.append(home + QStringLiteral("/Downloads/176 XMB WAVES for 5.00/176 XMB WAVES"));
    candidates.append(home + QStringLiteral("/Documents/176 XMB WAVES for 5.00/176 XMB WAVES"));

    for (const QString &candidate : candidates) {
        if (collectionHasWaves(candidate))
            return candidate;
    }
    return {};
}

bool WaveScanner::rcoFileExists(const QString &path)
{
    const QString normalized = path.trimmed().remove(QStringLiteral("file://"));
    return normalized.length() > 2 && QFileInfo::exists(normalized);
}

QString WaveScanner::previewImageFor(const QString &waveName) const
{
    if (m_collectionPath.isEmpty())
        return {};
    const QDir dir(QDir(m_collectionPath).filePath(waveName));
    for (const QString &name : {QStringLiteral("screen3.bmp"), QStringLiteral("preview.png"),
                                QStringLiteral("screenshot.png")}) {
        const QString path = dir.filePath(name);
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

bool WaveScanner::loadCache(QStringList *waves) const
{
    const QString cachePath = cacheFilePath(m_collectionPath);
    QFile f(cachePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("mtime")).toInteger() != collectionMtime(m_collectionPath))
        return false;

    const QJsonArray arr = obj.value(QStringLiteral("waves")).toArray();
    waves->clear();
    for (const auto &v : arr)
        waves->append(v.toString());
    return !waves->isEmpty();
}

void WaveScanner::saveCache(const QStringList &waves) const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("mtime"), collectionMtime(m_collectionPath));
    QJsonArray arr;
    for (const QString &w : waves)
        arr.append(w);
    obj.insert(QStringLiteral("waves"), arr);

    QFile f(cacheFilePath(m_collectionPath));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void WaveScanner::refresh()
{
    m_waves.clear();
    if (m_collectionPath.isEmpty()) {
        emit wavesChanged();
        return;
    }

    if (!loadCache(&m_waves)) {
        QDir dir(m_collectionPath);
        for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (QFileInfo(dir.filePath(entry + QStringLiteral("/system_plugin_bg.rco"))).isFile())
                m_waves.append(entry);
        }
        saveCache(m_waves);
    }

    emit wavesChanged();
    if (!m_waves.contains(m_selectedWave)) {
        if (!m_waves.isEmpty())
            setSelectedWave(m_waves.first());
        else
            m_selectedWave.clear();
    }
}

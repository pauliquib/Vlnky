// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wave_scanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

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

// --- GPU preference ------------------------------------------------------------------------------
//
// plasmashell is a single QRhi scene graph; a QQuickRhiItem cannot choose its
// physical device. The only workable per-user switch is an environment override
// applied to the plasmashell service itself: VK_ICD_FILENAMES picks the Vulkan
// ICD (Qt RHI Vulkan backend) and __EGL_VENDOR_LIBRARY_FILENAMES picks the GLVND
// vendor (Qt RHI OpenGL backend). Written as a systemd user drop-in so it
// survives restarts; "automatic" deletes it.

namespace {

QString pickFile(const QString &dirPath, const QStringList &keywords)
{
    const QDir dir(dirPath);
    for (const QString &kw : keywords) {
        const QStringList hits = dir.entryList({kw + QStringLiteral("*.json")}, QDir::Files);
        for (const QString &h : hits)
            if (h.contains(QStringLiteral("x86_64")))
                return dir.filePath(h);
        if (!hits.isEmpty())
            return dir.filePath(hits.first());
    }
    return QString();
}

QString gpuDropInPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
        + QStringLiteral("/.config/systemd/user/plasma-plasmashell.service.d/50-vlnky-gpu.conf");
}

} // namespace

QStringList WaveScanner::detectedGpus()
{
    QStringList out;
    const QDir dir(QStringLiteral("/sys/class/drm"));
    const QStringList nodes = dir.entryList({QStringLiteral("renderD*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &node : nodes) {
        const QString dev = dir.filePath(node + QStringLiteral("/device"));
        const QString driver = QFileInfo(dev + QStringLiteral("/driver")).symLinkTarget()
            .section(QLatin1Char('/'), -1);
        const QString pci = QFileInfo(dev).symLinkTarget().section(QLatin1Char('/'), -1);
        out.append(QStringLiteral("%1 — %2 (%3)").arg(node, driver, pci));
    }
    return out;
}

QString WaveScanner::applyGpuPreference(int pref)
{
    const QString path = gpuDropInPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    if (pref == 0) {
        QFile::remove(path);
        QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
        return QStringLiteral("automatic");
    }

    QString vkIcd;
    QString eglJson;
    if (pref == 1) { // integrated
        vkIcd = pickFile(QStringLiteral("/usr/share/vulkan/icd.d"),
                         {QStringLiteral("intel_icd"), QStringLiteral("intel_hasvk_icd"),
                          QStringLiteral("radeon_icd"), QStringLiteral("lvp_icd")});
        eglJson = pickFile(QStringLiteral("/usr/share/glvnd/egl_vendor.d"),
                           {QStringLiteral("*mesa")});
        if (eglJson.isEmpty())
            eglJson = QStringLiteral("/usr/share/glvnd/egl_vendor.d/50_mesa.json");
    } else { // dedicated
        vkIcd = pickFile(QStringLiteral("/usr/share/vulkan/icd.d"),
                         {QStringLiteral("nvidia_icd"), QStringLiteral("radeon_icd"),
                          QStringLiteral("nouveau_icd")});
        eglJson = pickFile(QStringLiteral("/usr/share/glvnd/egl_vendor.d"),
                           {QStringLiteral("*nvidia")});
        if (eglJson.isEmpty())
            eglJson = QStringLiteral("/usr/share/glvnd/egl_vendor.d/10_nvidia.json");
    }

    QStringList env;
    if (!vkIcd.isEmpty())
        env << QStringLiteral("VK_ICD_FILENAMES=") + vkIcd;
    if (!eglJson.isEmpty())
        env << QStringLiteral("__EGL_VENDOR_LIBRARY_FILENAMES=") + eglJson;
    if (pref == 2)
        env << QStringLiteral("DRI_PRIME=1");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    QTextStream ts(&f);
    ts << "[Service]\n";
    for (const QString &e : env)
        ts << "Environment=\"" << e << "\"\n";
    f.close();

    QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
    return env.join(QLatin1Char(' '));
}

void WaveScanner::restartPlasmashell()
{
    QProcess::startDetached(QStringLiteral("systemctl"),
                            {QStringLiteral("--user"), QStringLiteral("restart"),
                             QStringLiteral("plasma-plasmashell.service")});
}

// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../libxmbwave/psp_wave.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QQuickRhiItem>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

enum class XmbDebugRenderMode : int {
    Normal = 0,
    Wireframe = 1,
    TextureOnly = 2,
    ControlPoints = 3,
};

/// Snapshot handed from the GUI thread to the render thread in synchronize().
struct XmbWaveRenderData {
    std::vector<float> vertices; // pos(3) + normal(3)
    std::shared_ptr<const std::vector<uint32_t>> indices;
    std::shared_ptr<const std::vector<uint32_t>> lineIndices;
    quint64 topologySerial = 0;
    std::shared_ptr<const std::vector<uint8_t>> texture; // RGBA8
    int texW = 0;
    int texH = 0;
    quint64 textureSerial = 0;
    float color[4] = {1.f, 1.f, 1.f, 1.f}; // rgb multiplier, a unused
    float verticalCenter = 0.5f;
    float fovY = 30.f;
    XmbDebugRenderMode debugMode = XmbDebugRenderMode::Normal;
    bool loaded = false;

    // Quality / post-processing
    float renderScale = 1.f; // supersampling of the wave layer (1 .. 2)
    int msaaSamples = 4;     // MSAA of the wave layer (1 = off)
    float bloom = 0.35f;     // glow strength (0 = off)
    bool bicubic = true;     // smooth reflection-map magnification
    float dither = 1.f;      // output dither amplitude (in 8-bit steps)
    float vignette = 0.f;

    // Background drawn by the renderer itself (dithered, no banding). 0 = none (QML draws it).
    int backgroundMode = 0;  // 0 none, 1 gradient, 2 XMB theme colour
    float bgTop[3] = {0.f, 0.f, 0.f};
    float bgBottom[3] = {0.f, 0.f, 0.f};
    float bgAngle = 90.f;    // degrees, gradient mode
    float brightness = 1.f;

    // Procedural PS2 (PSX DESR) wave instead of the RCO mesh.
    bool ps2Wave = false;
    float ps2Color[3] = {1.f, 1.f, 1.f};
    float ps2Opacity = 1.f;

    // Procedural Svec Studio wave (the simple Sencurio landing-page hero waves).
    bool svecWave = false;
    float svec[8] = {}; // see composite.frag for the layout

    float time = 0.f; // seconds
};

class XmbWaveRhiItem : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QString rcoPath READ rcoPath WRITE setRcoPath NOTIFY rcoPathChanged)
    Q_PROPERTY(QString collectionPath READ collectionPath WRITE setCollectionPath NOTIFY collectionPathChanged)
    Q_PROPERTY(QString waveName READ waveName WRITE setWaveName NOTIFY waveNameChanged)
    Q_PROPERTY(int tessLevel READ tessLevel WRITE setTessLevel NOTIFY tessLevelChanged)
    Q_PROPERTY(int effectiveTessLevel READ effectiveTessLevel NOTIFY effectiveTessLevelChanged)
    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool pauseOnBattery READ pauseOnBattery WRITE setPauseOnBattery NOTIFY pauseOnBatteryChanged)
    Q_PROPERTY(bool batterySaver READ batterySaver WRITE setBatterySaver NOTIFY batterySaverChanged)
    Q_PROPERTY(bool adaptiveQuality READ adaptiveQuality WRITE setAdaptiveQuality NOTIFY adaptiveQualityChanged)
    Q_PROPERTY(qreal waveOpacity READ waveOpacity WRITE setWaveOpacity NOTIFY waveOpacityChanged)
    Q_PROPERTY(qreal waveCenterY READ waveCenterY WRITE setWaveCenterY NOTIFY waveCenterYChanged)
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(qreal phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(int errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString loadStatus READ loadStatus NOTIFY loadStatusChanged)
    Q_PROPERTY(bool debugOverlay READ debugOverlay WRITE setDebugOverlay NOTIFY debugOverlayChanged)
    Q_PROPERTY(bool debugOverlayVisible READ debugOverlayEnabled NOTIFY debugOverlayChanged)
    Q_PROPERTY(int debugRenderMode READ debugRenderMode WRITE setDebugRenderMode NOTIFY debugRenderModeChanged)
    Q_PROPERTY(bool wireframeMode READ wireframeMode WRITE setWireframeMode NOTIFY debugRenderModeChanged)
    Q_PROPERTY(qreal renderScale READ renderScale WRITE setRenderScale NOTIFY renderQualityChanged)
    Q_PROPERTY(int msaaSamples READ msaaSamples WRITE setMsaaSamples NOTIFY renderQualityChanged)
    Q_PROPERTY(qreal bloom READ bloom WRITE setBloom NOTIFY renderQualityChanged)
    Q_PROPERTY(bool bicubicTexture READ bicubicTexture WRITE setBicubicTexture NOTIFY renderQualityChanged)
    Q_PROPERTY(bool dither READ dither WRITE setDither NOTIFY renderQualityChanged)
    Q_PROPERTY(qreal vignette READ vignette WRITE setVignette NOTIFY renderQualityChanged)
    Q_PROPERTY(int backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY backgroundChanged)
    Q_PROPERTY(QColor backgroundTop READ backgroundTop WRITE setBackgroundTop NOTIFY backgroundChanged)
    Q_PROPERTY(QColor backgroundBottom READ backgroundBottom WRITE setBackgroundBottom NOTIFY backgroundChanged)
    Q_PROPERTY(qreal backgroundAngle READ backgroundAngle WRITE setBackgroundAngle NOTIFY backgroundChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY backgroundChanged)
    Q_PROPERTY(int waveStyle READ waveStyle WRITE setWaveStyle NOTIFY waveStyleChanged)
    Q_PROPERTY(QColor ps2WaveColor READ ps2WaveColor WRITE setPs2WaveColor NOTIFY waveStyleChanged)
    Q_PROPERTY(QColor waveTint READ waveTint WRITE setWaveTint NOTIFY waveStyleChanged)
    Q_PROPERTY(QColor svecWaveColor READ svecWaveColor WRITE setSvecWaveColor NOTIFY waveStyleChanged)
    Q_PROPERTY(bool svecWaveCustom READ svecWaveCustom WRITE setSvecWaveCustom NOTIFY waveStyleChanged)

public:
    explicit XmbWaveRhiItem(QQuickItem *parent = nullptr);
    ~XmbWaveRhiItem() override;

    QString rcoPath() const { return m_rcoPath; }
    void setRcoPath(const QString &path);
    QString collectionPath() const { return m_collectionPath; }
    void setCollectionPath(const QString &path);
    QString waveName() const { return m_waveName; }
    void setWaveName(const QString &name);

    int tessLevel() const { return m_tessLevel; }
    void setTessLevel(int level);
    int effectiveTessLevel() const { return m_effectiveTessLevel; }
    int maxFps() const { return m_maxFps; }
    void setMaxFps(int fps);
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    bool pauseOnBattery() const { return m_pauseOnBattery; }
    void setPauseOnBattery(bool p);
    bool batterySaver() const { return m_batterySaver; }
    void setBatterySaver(bool v);
    bool adaptiveQuality() const { return m_adaptiveQuality; }
    void setAdaptiveQuality(bool v);
    qreal waveOpacity() const { return m_waveOpacity; }
    void setWaveOpacity(qreal o);
    qreal waveCenterY() const { return m_waveCenterY; }
    void setWaveCenterY(qreal c);
    qreal speed() const { return m_speed; }
    void setSpeed(qreal s);

    bool loaded() const { return m_loaded; }
    bool loading() const { return m_loading; }
    qreal phase() const { return qreal(m_frame); }
    int errorCode() const { return m_errorString.isEmpty() ? 0 : 1; }
    QString errorString() const { return m_errorString; }
    QString diagnostics() const { return m_diagnostics; }
    QString loadStatus() const { return m_loadStatus; }

    bool debugOverlay() const { return m_debugOverlay; }
    void setDebugOverlay(bool v);
    bool debugOverlayEnabled() const;
    int debugRenderMode() const { return int(m_debugMode); }
    void setDebugRenderMode(int mode);
    bool wireframeMode() const { return m_debugMode == XmbDebugRenderMode::Wireframe; }
    void setWireframeMode(bool v);

    qreal renderScale() const { return m_renderScale; }
    void setRenderScale(qreal s);
    int msaaSamples() const { return m_msaaSamples; }
    void setMsaaSamples(int n);
    qreal bloom() const { return m_bloom; }
    void setBloom(qreal b);
    bool bicubicTexture() const { return m_bicubic; }
    void setBicubicTexture(bool v);
    bool dither() const { return m_dither; }
    void setDither(bool v);
    qreal vignette() const { return m_vignette; }
    void setVignette(qreal v);

    int backgroundMode() const { return m_backgroundMode; }
    void setBackgroundMode(int m);
    QColor backgroundTop() const { return m_bgTop; }
    void setBackgroundTop(const QColor &c);
    QColor backgroundBottom() const { return m_bgBottom; }
    void setBackgroundBottom(const QColor &c);
    qreal backgroundAngle() const { return m_bgAngle; }
    void setBackgroundAngle(qreal a);
    qreal brightness() const { return m_brightness; }
    void setBrightness(qreal b);

    /// 0 = PSP wave from the RCO, 1 = procedural PS2 (PSX DESR) wave, 2 = Svec Studio wave.
    int waveStyle() const { return m_waveStyle; }
    void setWaveStyle(int s);
    QColor ps2WaveColor() const { return m_ps2Color; }
    void setPs2WaveColor(const QColor &c);
    /// Multiplies the RCO wave colour (white = original).
    QColor waveTint() const { return m_waveTint; }
    void setWaveTint(const QColor &c);
    /// Wave colour of the Svec Studio wave.
    QColor svecWaveColor() const { return m_svecWaveColor; }
    void setSvecWaveColor(const QColor &c);
    /// true = user-picked wave colour (custom palette), false = theme-derived.
    bool svecWaveCustom() const { return m_svecWaveCustom; }
    void setSvecWaveCustom(bool v);

    Q_INVOKABLE static QString defaultScreenshotDir();
    Q_INVOKABLE void captureScreenshot(const QString &label = QString());
    Q_INVOKABLE void captureAllDebugScreenshots();
    Q_INVOKABLE QStringList scanWaves() const;
    Q_INVOKABLE bool reload();

    void syncToRenderer(XmbWaveRenderData &out) const;

signals:
    void rcoPathChanged();
    void collectionPathChanged();
    void waveNameChanged();
    void tessLevelChanged();
    void effectiveTessLevelChanged();
    void maxFpsChanged();
    void pausedChanged();
    void pauseOnBatteryChanged();
    void batterySaverChanged();
    void adaptiveQualityChanged();
    void waveOpacityChanged();
    void waveCenterYChanged();
    void speedChanged();
    void loadedChanged();
    void loadingChanged();
    void phaseChanged();
    void errorChanged();
    void loadError(const QString &message);
    void diagnosticsChanged();
    void loadStatusChanged();
    void debugOverlayChanged();
    void debugRenderModeChanged();
    void renderQualityChanged();
    void backgroundChanged();
    void waveStyleChanged();
    void screenshotCaptured(const QString &path);
    void allScreenshotsCaptured(const QString &directory);

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    void tick();
    void startAsyncLoad();
    void setLoadStatus(const QString &s);
    void setErrorString(const QString &e);
    void updateTimer();
    void updateEffectiveTessLevel();
    void rebuildTopology();
    void evaluateMesh();
    void updateDiagnostics();
    bool animating() const;
    bool proceduralActive() const { return m_waveStyle != 0; }

    QString m_rcoPath;
    QString m_collectionPath;
    QString m_waveName;
    int m_tessLevel = 2;
    int m_effectiveTessLevel = 2;
    int m_maxFps = 30;
    bool m_paused = false;
    bool m_pauseOnBattery = true;
    bool m_onBattery = false;
    bool m_batterySaver = false;
    bool m_adaptiveQuality = true;
    qreal m_waveOpacity = 0.92;
    qreal m_waveCenterY = 0.5;
    qreal m_speed = 1.0;
    bool m_debugOverlay = false;
    XmbDebugRenderMode m_debugMode = XmbDebugRenderMode::Normal;

    qreal m_renderScale = 1.0;
    int m_msaaSamples = 4;
    qreal m_bloom = 0.35;
    bool m_bicubic = true;
    bool m_dither = true;
    qreal m_vignette = 0.0;
    int m_backgroundMode = 0;
    QColor m_bgTop = QColor(8, 12, 28);
    QColor m_bgBottom = QColor(20, 40, 90);
    qreal m_bgAngle = 90.0;
    qreal m_brightness = 1.0;
    int m_waveStyle = 0;
    QColor m_ps2Color = QColor(255, 255, 255);
    QColor m_waveTint = QColor(255, 255, 255);
    QColor m_svecWaveColor = QColor(0x26, 0xc6, 0xda);
    bool m_svecWaveCustom = false;
    double m_time = 0.0;

    bool m_loaded = false;
    bool m_loading = false;
    QString m_loadStatus = QStringLiteral("loading");
    QString m_errorString;
    QString m_diagnostics;
    std::atomic<int> m_loadGeneration{0};

    std::shared_ptr<xmb::PspWave> m_wave;
    xmb::PspWaveTessellator m_tess;
    float m_frame = 0.f;

    QTimer m_timer;
    QTimer m_batteryTimer;
    QElapsedTimer m_clock;
    qint64 m_lastTickNs = 0;
    std::vector<float> m_frameCostMs;
    float m_fps = 0.f;
    qint64 m_fpsWindowStart = 0;
    int m_fpsFrames = 0;

    XmbWaveRenderData m_cache;
    quint64 m_topologySerial = 0;
    quint64 m_textureSerial = 0;
};

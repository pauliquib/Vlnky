// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.taskmanager as TaskManager
import "org/psvec/vlnky"

WallpaperItem {
    id: root

    readonly property bool debugEnabled: root.configuration
        ? (root.configuration.DebugOverlay === true) : false

    readonly property int bgMode: root.configuration
        ? Number(root.configuration.BackgroundMode) : 1
    readonly property real waveHeightRatio: {
        if (!root.configuration)
            return 0.58
        var r = Number(root.configuration.WaveHeightRatio)
        if (isNaN(r) || r < 0.3)
            return 0.58
        return Math.min(0.8, r)
    }
    readonly property real waveOpacityValue: {
        if (!root.configuration)
            return 0.92
        var o = Number(root.configuration.WaveOpacity)
        if (isNaN(o) || o < 0.2)
            return 0.92
        return o
    }
    readonly property string defaultCollection: waveScanner.resolveDefaultCollection()

    function cfg(name, fallback) {
        if (!root.configuration || root.configuration[name] === undefined || root.configuration[name] === null)
            return fallback
        return root.configuration[name]
    }

    // XMB month colours shared by the PSP, PSX (DESR) and PS3 system menus: January .. December.
    readonly property var xmbMonthColors: [
        "#cbcbcb", "#d8bf1a", "#6db217", "#e17e9a", "#178816", "#9a61c8",
        "#02cdc7", "#0c76c0", "#b444c0", "#e5a708", "#875b1e", "#e3412a"
    ]

    // Refreshed every minute: month rollover and time-of-day brightness.
    property date now: new Date()
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.now = new Date()
    }

    // Power modes: 0 = normal, 1 = low power (cheapest combination of the existing
    // quality knobs), 2 = static frame (paused forever; renders once per change).
    readonly property int powerMode: Number(cfg("PowerMode", 0))
    readonly property bool lowPower: powerMode === 1
    readonly property bool pauseWhenCovered: cfg("PauseWhenCovered", true) !== false
    property bool desktopCovered: false

    // Role numbers in TaskManager::AbstractTasksModel (Qt::UserRole + N).
    readonly property int roleIsMaximized: 277
    readonly property int roleIsFullScreen: 283

    function updateCovered() {
        if (!root.pauseWhenCovered) {
            root.desktopCovered = false
            return
        }
        for (var i = 0; i < tasksModel.count; ++i) {
            const idx = tasksModel.index(i, 0)
            if (tasksModel.data(idx, root.roleIsFullScreen) === true
                || tasksModel.data(idx, root.roleIsMaximized) === true) {
                root.desktopCovered = true
                return
            }
        }
        root.desktopCovered = false
    }

    // Windows on this screen only (virtual desktop and minimized windows filtered out).
    // On Wayland a maximized/fullscreen window on the current desktop is the closest
    // practical approximation of "the wallpaper is fully hidden".
    TaskManager.TasksModel {
        id: tasksModel
        filterByScreen: true
        filterByVirtualDesktop: true
        filterNotMinimized: true
        screenGeometry: waveEngine.screenGeometry
        onCountChanged: root.updateCovered()
        onDataChanged: root.updateCovered()
        onScreenGeometryChanged: root.updateCovered()
    }

    readonly property int colorTheme: Number(cfg("ColorTheme", 0))
    readonly property bool themed: colorTheme > 0
    readonly property color themeColor: {
        if (colorTheme === 14)
            return cfg("CustomThemeColor", "#3fa9f5")
        var m = colorTheme === 1 ? now.getMonth() : Math.max(0, Math.min(11, colorTheme - 2))
        return xmbMonthColors[m]
    }

    // Night dimming modelled after the PSX/PS3 XMB: full brightness 12:00-15:00, darkest 22:00-06:00.
    readonly property real dayBrightness: {
        if (!themed || cfg("TimeOfDayBrightness", true) === false)
            return 1.0
        var h = now.getHours() + now.getMinutes() / 60
        var night
        if (h >= 12 && h < 15) night = 0
        else if (h >= 22 || h < 6) night = 1
        else if (h >= 15) night = (h - 15) / 7
        else night = 1 - (h - 6) / 6
        return 1.0 - 0.5 * night
    }

    readonly property bool imageBackground: bgMode === 2
        || (bgMode === 3 && waveFolderBackground.length > 0)
    // The renderer draws gradients itself (dithered, no 8-bit banding on large screens).
    readonly property int engineBackgroundMode: themed ? 2 : (imageBackground ? 0 : 1)
    readonly property color engineBgTop: themed ? themeColor
        : (bgMode === 0 ? cfg("BackgroundColor", "#080c1c") : cfg("GradientStart", "#080c1c"))
    readonly property color engineBgBottom: themed ? themeColor
        : (bgMode === 0 ? cfg("BackgroundColor", "#080c1c") : cfg("GradientEnd", "#14285a"))

    readonly property color ps2Color: {
        var c = themed ? themeColor : engineBgBottom
        return Qt.rgba(Math.min(1, c.r + 65 / 255), Math.min(1, c.g + 65 / 255), Math.min(1, c.b + 90 / 255), 1)
    }

    // Custom wave colour shared by all styles (white tint = original RCO colours).
    readonly property bool customWaveColor: Number(cfg("WaveColorMode", 0)) === 1
    readonly property color waveColorCustom: cfg("WaveColor", "#26c6da")

    // Svec Studio wave colour: the custom pick, the XMB theme colour, or the
    // default svec-studio accent (#26c6da, cf. defaultWaveColor()).
    readonly property color svecColor: root.customWaveColor ? root.waveColorCustom
        : (root.themed ? root.themeColor : "#26c6da")

    function rcoIn(base, wave) {
        return base.length > 0 && wave.length > 0 ? base + "/" + wave + "/system_plugin_bg.rco" : ""
    }

    // Resolve the RCO robustly: the stored absolute path may point to a drive that is no longer
    // mounted, so fall back to the collection folder and finally the installed default collection.
    readonly property string effectiveRcoPath: {
        if (!root.configuration)
            return ""
        var direct = String(root.configuration.RcoPath || "").replace("file://", "").trim()
        var base = String(root.configuration.CollectionPath || "").replace("file://", "").trim()
        var wave = String(root.configuration.WaveName || "").trim()
        if (direct.length > 2 && direct.endsWith(".rco") && waveScanner.rcoFileExists(direct))
            return direct
        var candidate = rcoIn(base, wave)
        if (candidate.length > 0 && waveScanner.rcoFileExists(candidate))
            return candidate
        if (wave.length === 0 && direct.endsWith(".rco")) {
            var parts = direct.split("/")
            if (parts.length >= 2)
                wave = parts[parts.length - 2]
        }
        candidate = rcoIn(root.defaultCollection, wave)
        if (candidate.length > 0 && waveScanner.rcoFileExists(candidate))
            return candidate
        return direct.length > 2 ? direct : rcoIn(base, wave)
    }

    readonly property string waveFolderBackground: {
        if (!root.configuration || root.bgMode !== 3)
            return ""
        var base = String(root.configuration.CollectionPath || "").replace("file://", "").trim()
        var wave = String(root.configuration.WaveName || "").trim()
        if (base.length === 0 || wave.length === 0)
            return ""
        return waveScanner.previewImageFor(wave)
    }

    WaveScanner {
        id: waveScanner
        collectionPath: root.configuration
            ? String(root.configuration.CollectionPath || "").replace("file://", "").trim()
            : ""
    }

    Rectangle {
        anchors.fill: parent
        visible: root.bgMode === 0 && !waveEngine.visible
        color: root.configuration ? root.configuration.BackgroundColor : "#080c1c"
    }

    Rectangle {
        anchors.fill: parent
        visible: root.bgMode === 1 && !waveEngine.visible
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: root.configuration ? root.configuration.GradientStart : "#080c1c"
            }
            GradientStop {
                position: 1.0
                color: root.configuration ? root.configuration.GradientEnd : "#14285a"
            }
        }
    }

    Image {
        anchors.fill: parent
        visible: root.bgMode === 2 && !root.themed
            && root.configuration
            && String(root.configuration.BackgroundImage || "").length > 0
        source: root.configuration ? root.configuration.BackgroundImage : ""
        fillMode: Image.PreserveAspectCrop
    }

    Image {
        anchors.fill: parent
        visible: root.bgMode === 3 && root.waveFolderBackground.length > 0 && !root.themed
        source: root.waveFolderBackground.length > 0
            ? "file://" + encodeURI(root.waveFolderBackground)
            : ""
        fillMode: Image.PreserveAspectCrop
    }

    Rectangle {
        anchors.fill: parent
        visible: root.bgMode === 3 && root.waveFolderBackground.length === 0 && !waveEngine.visible
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: root.configuration ? root.configuration.GradientStart : "#080c1c"
            }
            GradientStop {
                position: 1.0
                color: root.configuration ? root.configuration.GradientEnd : "#14285a"
            }
        }
    }

    XmbWaveItem {
        id: waveEngine
        z: 1
        anchors.fill: parent
        // "Wave height" slider: 0.3 (low on screen) .. 0.8 (high); centre of the wave band.
        waveCenterY: 1.0 - root.waveHeightRatio / 2.0
        rcoPath: root.effectiveRcoPath
        tessLevel: root.lowPower ? 1 : (root.configuration ? (root.configuration.TessLevel || 3) : 3)
        maxFps: root.lowPower ? 10 : (root.configuration ? (root.configuration.MaxFps || 30) : 30)
        adaptiveQuality: !root.lowPower
            && (root.configuration ? (root.configuration.AdaptiveQuality !== false) : true)
        paused: root.powerMode === 2 || (root.pauseWhenCovered && root.desktopCovered)
        pauseOnBattery: root.configuration ? (root.configuration.PauseOnBattery !== false) : false
        batterySaver: root.configuration ? (root.configuration.BatterySaver === true) : false
        waveOpacity: waveOpacityValue
        speed: {
            var v = Number(root.cfg("WaveSpeed", 1.0))
            return isNaN(v) || v <= 0 ? 1.0 : v
        }
        debugOverlay: root.debugEnabled
        wireframeMode: root.configuration ? (root.configuration.WireframeMode === true) : false

        waveStyle: Number(root.cfg("WaveStyle", 0))
        ps2WaveColor: root.customWaveColor ? root.waveColorCustom : root.ps2Color
        waveTint: root.customWaveColor ? root.waveColorCustom : "#ffffff"
        svecWaveColor: root.svecColor
        svecWaveCustom: root.customWaveColor
        svecWaveHeight: {
            var v = Number(root.cfg("SvecWaveHeight", 1.0))
            return isNaN(v) || v <= 0 ? 1.0 : Math.min(2.5, v)
        }
        backgroundMode: root.engineBackgroundMode
        backgroundTop: root.engineBgTop
        backgroundBottom: root.engineBgBottom
        backgroundAngle: Number(root.cfg("GradientAngle", 90))
        brightness: root.dayBrightness

        // Quality / post-processing. Battery saver and the low-power mode both fall
        // back to the cheap path (1x scale, no MSAA, no bloom, coarse mesh).
        // The item's own texture is halved in low power: the whole frame graph
        // (wave scene, composite) runs at half resolution, the scene graph upscales.
        fixedColorBufferWidth: root.lowPower ? Math.round(width * Screen.devicePixelRatio * 0.5) : 0
        fixedColorBufferHeight: root.lowPower ? Math.round(height * Screen.devicePixelRatio * 0.5) : 0
        renderScale: root.lowPower ? 0.5 : (batterySaver ? 1.0 : Number(root.cfg("RenderScale", 1.0)))
        msaaSamples: (batterySaver || root.lowPower) ? 1 : Number(root.cfg("MsaaSamples", 4))
        bloom: (batterySaver || root.lowPower) ? 0.0 : Number(root.cfg("Bloom", 0.3))
        bicubicTexture: !root.lowPower && root.cfg("BicubicTexture", true) !== false
        dither: root.cfg("Dither", true) !== false
        vignette: root.lowPower ? 0.0 : Number(root.cfg("Vignette", 0.0))

        visible: height > 8 && (root.effectiveRcoPath.length > 2 || waveStyle > 0 || backgroundMode > 0)

        onAllScreenshotsCaptured: function(dir) {
            screenshotToast.text = i18nd("plasma_wallpaper_org.psvec.vlnky",
                                         "Screenshots saved to %1", dir)
            screenshotToast.visible = true
            screenshotHide.restart()
        }
    }

    Rectangle {
        z: 3
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        width: Math.min(parent.width - 16, diagText.width + 16)
        height: diagText.height + 12
        radius: 4
        color: "#cc000000"
        visible: waveEngine.debugOverlayVisible && root.effectiveRcoPath.length > 2

        Text {
            id: diagText
            anchors.centerIn: parent
            width: Math.min(parent.parent.width - 32, 520)
            color: "#a8e8ff"
            font.family: "monospace"
            font.pixelSize: 10
            wrapMode: Text.Wrap
            text: waveEngine.diagnostics
        }
    }

    Rectangle {
        z: 2
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        width: debugText.width + 16
        height: debugText.height + 8
        radius: 4
        color: "#cc000000"
        visible: root.effectiveRcoPath.length > 2
            && !waveEngine.loading
            && !waveEngine.debugOverlayVisible
            && (waveEngine.loadStatus === "error"
                || waveEngine.loadStatus === "fallback")
            && !waveEngine.loaded

        Text {
            id: debugText
            anchors.centerIn: parent
            color: "#ffcc88"
            font.pixelSize: 11
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave: %1", root.effectiveRcoPath)
                + "\nstatus=" + waveEngine.loadStatus
                + " err=" + waveEngine.errorCode + " " + waveEngine.errorString
        }
    }

    Rectangle {
        id: screenshotToast
        z: 4
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 24
        width: toastText.width + 24
        height: toastText.height + 12
        radius: 4
        color: "#dd102040"
        visible: false

        property alias text: toastText.text

        Text {
            id: toastText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 11
        }

        Timer {
            id: screenshotHide
            interval: 4000
            onTriggered: screenshotToast.visible = false
        }
    }

    Shortcut {
        sequence: "F12"
        enabled: root.effectiveRcoPath.length > 2
        onActivated: waveEngine.captureAllDebugScreenshots()
    }
}

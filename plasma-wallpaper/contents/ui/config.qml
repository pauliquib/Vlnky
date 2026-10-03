// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kirigami.private as KirigamiPrivate
import org.kde.kquickcontrols as KQuickControls
import "org/psvec/vlnky"

Kirigami.FormLayout {
    id: root
    twinFormLayouts: parentLayout
    property alias formLayout: root

    property var configDialog
    property var wallpaperConfiguration

    property string cfg_CollectionPath: ""
    property string cfg_WaveName: ""
    property string cfg_RcoPath: ""
    property string cfg_TexturePath: ""
    property int cfg_TessLevel: 3
    property int cfg_WaveStyle: 0
    property int cfg_WaveColorMode: 0
    property color cfg_WaveColor: "#26c6da"
    property int cfg_ColorTheme: 0
    property color cfg_CustomThemeColor: "#3fa9f5"
    property bool cfg_TimeOfDayBrightness: true
    property real cfg_RenderScale: 1.0
    property int cfg_MsaaSamples: 4
    property real cfg_Bloom: 0.3
    property bool cfg_BicubicTexture: true
    property bool cfg_Dither: true
    property real cfg_Vignette: 0.0
    property int cfg_MaxFps: 30
    property bool cfg_PauseOnBattery: true
    property bool cfg_BatterySaver: false
    property int cfg_BackgroundMode: 1
    property color cfg_BackgroundColor: "#080c1c"
    property color cfg_GradientStart: "#080c1c"
    property color cfg_GradientEnd: "#14285a"
    property real cfg_GradientAngle: 90
    property url cfg_BackgroundImage
    property real cfg_WaveOpacity: 0.92
    property real cfg_WaveHeightRatio: 0.58
    property real cfg_WaveSpeed: 1.0
    property bool cfg_AdaptiveQuality: true
    property bool cfg_DebugOverlay: false
    property bool cfg_WireframeMode: false

    // Preferred width of the right-hand column; controls shrink below it on narrow dialogs.
    readonly property real fieldWidth: Kirigami.Units.gridUnit * 18
    property bool showAdvanced: false

    // XMB month colours (PSP / PSX / PS3), January .. December.
    readonly property var xmbMonthColors: ["#cbcbcb", "#d8bf1a", "#6db217", "#e17e9a", "#178816", "#9a61c8", "#02cdc7", "#0c76c0", "#b444c0", "#e5a708", "#875b1e", "#e3412a"]
    readonly property int customThemeIndex: 14
    readonly property color previewThemeColor: {
        if (cfg_ColorTheme === customThemeIndex)
            return cfg_CustomThemeColor
        if (cfg_ColorTheme === 1)
            return xmbMonthColors[new Date().getMonth()]
        return xmbMonthColors[Math.max(0, Math.min(11, cfg_ColorTheme - 2))]
    }

    readonly property var themeNames: [i18nd("plasma_wallpaper_org.psvec.vlnky", "Off – use my background"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Automatic – colour of the current month"), i18nd("plasma_wallpaper_org.psvec.vlnky", "January (silver)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "February (gold)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "March (lime)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "April (pink)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "May (green)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "June (purple)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "July (turquoise)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "August (blue)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "September (violet)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "October (orange)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "November (brown)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "December (red)"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom colour…")]

    function themeSwatch(index) {
        if (index <= 0)
            return "transparent"
        if (index === customThemeIndex)
            return cfg_CustomThemeColor
        return index === 1 ? xmbMonthColors[new Date().getMonth()] : xmbMonthColors[index - 2]
    }

    // Effective wave colour per style: the custom colour, the Svec Studio default
    // accent / theme colour, or the PS2 colour derived from the theme/background.
    readonly property color waveColorEffective: cfg_WaveColorMode === 1 ? cfg_WaveColor
        : (cfg_WaveStyle === 2 ? (cfg_ColorTheme > 0 ? previewThemeColor : "#26c6da")
        : Qt.rgba(Math.min(1, previewThemeColor.r + 0.255), Math.min(1, previewThemeColor.g + 0.255), Math.min(1, previewThemeColor.b + 0.353), 1))

    readonly property var backgroundPresets: [
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Midnight blue (default)"),
            top: "#080c1c",
            bottom: "#14285a"
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Deep ocean"),
            top: "#021018",
            bottom: "#0b4a6e"
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Violet night"),
            top: "#0d0616",
            bottom: "#3c1a5e"
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Emerald"),
            top: "#03110a",
            bottom: "#115c3a"
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Sunset"),
            top: "#1a0508",
            bottom: "#8a3414"
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Graphite"),
            top: "#0c0c0e",
            bottom: "#3a3c42"
        }
    ]

    readonly property var qualityPresets: [
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Performance"),
            scale: 1.0,
            msaa: 1,
            bloom: 0.0,
            tess: 2,
            fps: 30
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Balanced"),
            scale: 1.0,
            msaa: 4,
            bloom: 0.3,
            tess: 3,
            fps: 60
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Beautiful (Full HD+)"),
            scale: 1.5,
            msaa: 4,
            bloom: 0.35,
            tess: 4,
            fps: 60
        },
        {
            label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Maximum (4K)"),
            scale: 2.0,
            msaa: 8,
            bloom: 0.4,
            tess: 4,
            fps: 60
        }
    ]

    readonly property int currentQualityPreset: {
        for (var i = 0; i < qualityPresets.length; ++i) {
            var p = qualityPresets[i]
            if (Math.abs(cfg_RenderScale - p.scale) < 0.01 && cfg_MsaaSamples === p.msaa && Math.abs(cfg_Bloom - p.bloom) < 0.01 && cfg_TessLevel === p.tess && cfg_MaxFps === p.fps)
                return i
        }
        return qualityPresets.length
        // "Custom"
    }

    function applyPreset(scale, msaa, bloom, tess, fps) {
        cfg_RenderScale = scale
        cfg_MsaaSamples = msaa
        cfg_Bloom = bloom
        cfg_TessLevel = tess
        cfg_MaxFps = fps
        cfg_BicubicTexture = true
        cfg_Dither = true
    }

    readonly property string defaultCollectionPath: waveScanner.resolveDefaultCollection()
    readonly property string extractScript: {
        const u = Qt.resolvedUrl("../code/xmb_extract.py")
        return u.toString().replace("file://", "")
    }
    readonly property string extractAllScript: {
        const u = Qt.resolvedUrl("../code/extract-all-waves.sh")
        return u.toString().replace("file://", "")
    }
    readonly property string extractCommand: cfg_RcoPath.length > 0 ? "python3 \"" + extractScript + "\" \"" + cfg_RcoPath + "\"" : ""
    readonly property bool textureMissing: cfg_WaveStyle === 0 && cfg_RcoPath.length > 0 && textureProbe.status !== Image.Ready

    function localFileUrl(path) {
        if (!path || path.length < 2)
            return ""
        var p = String(path).replace(/^file:\/\//, "")
        return "file://" + encodeURI(p)
    }

    function collectionFolderUrl() {
        var p = cfg_CollectionPath
        if (!p || p.length === 0)
            return ""
        return p.startsWith("file://") ? p : "file://" + p
    }

    function updateRcoPath() {
        if (!cfg_CollectionPath || !cfg_WaveName)
            return
        var base = cfg_CollectionPath.replace("file://", "").trim()
        if (base.length === 0)
            return
        if (cfg_WaveName.startsWith("imported_"))
            return
        if (cfg_RcoPath.length > 2 && cfg_RcoPath.endsWith(".rco") && cfg_RcoPath.indexOf(base + "/") !== 0 && waveScanner.rcoFileExists(cfg_RcoPath))
            return
        cfg_RcoPath = base + "/" + cfg_WaveName + "/system_plugin_bg.rco"
        cfg_TexturePath = base + "/" + cfg_WaveName + "/wave_texture.png"
    }

    function syncWaveComboIndex() {
        if (!cfg_WaveName || waveScanner.waves.length === 0)
            return
        const idx = waveScanner.waves.indexOf(cfg_WaveName)
        if (idx >= 0)
            waveCombo.currentIndex = idx
    }

    Component.onCompleted: {
        if (!cfg_CollectionPath || cfg_CollectionPath.length === 0 || !waveScanner.isValidCollection(cfg_CollectionPath))
            cfg_CollectionPath = defaultCollectionPath
        waveScanner.collectionPath = cfg_CollectionPath
        if (cfg_RcoPath.length > 2 && cfg_RcoPath.endsWith(".rco") && waveScanner.rcoFileExists(cfg_RcoPath)) {
            if (!cfg_WaveName || cfg_WaveName.length === 0)
                cfg_WaveName = ""
        } else if (waveScanner.waves.length > 0 && (!cfg_WaveName || cfg_WaveName.length === 0)) {
            cfg_WaveName = waveScanner.waves[0]
            updateRcoPath()
        } else {
            updateRcoPath()
        }
        syncWaveComboIndex()
    }

    WaveScanner {
        id: waveScanner
        onWavesChanged: syncWaveComboIndex()
    }

    // Slider + value label + optional reset button; fills the column but shrinks on narrow dialogs.
    component ValueSlider: RowLayout {
        id: vs
        property real from: 0.0
        property real to: 1.0
        property real stepSize: 0.05
        property real value: 0.0
        property real defaultValue: NaN
        property var formatValue: function (v) {
            return Math.round(v * 100) + " %"
        }
        signal edited(real v)

        Layout.fillWidth: true
        Layout.preferredWidth: Kirigami.Units.gridUnit * 18
        spacing: Kirigami.Units.smallSpacing

        Slider {
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 5
            from: vs.from
            to: vs.to
            stepSize: vs.stepSize
            value: vs.value
            onMoved: vs.edited(value)
        }
        Label {
            Layout.minimumWidth: Kirigami.Units.gridUnit * 2.5
            horizontalAlignment: Text.AlignRight
            text: vs.formatValue(vs.value)
        }
        ToolButton {
            visible: !isNaN(vs.defaultValue)
            enabled: Math.abs(vs.value - vs.defaultValue) > 0.001
            icon.name: "edit-reset"
            display: AbstractButton.IconOnly
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Reset to default")
            ToolTip.visible: hovered
            ToolTip.text: text
            onClicked: vs.edited(vs.defaultValue)
        }
    }

    // Colour swatch button (opens the colour picker) + hex code.
    component ColorField: RowLayout {
        id: cf
        property color value
        property string dialogTitle
        signal edited(color c)
        spacing: Kirigami.Units.smallSpacing

        KQuickControls.ColorButton {
            color: cf.value
            dialogTitle: cf.dialogTitle
            showAlphaChannel: false
            onAccepted: color => cf.edited(color)
        }
        Label {
            text: String(cf.value).toUpperCase()
            opacity: 0.7
        }
    }

    // ───────────────────────── Wave ─────────────────────────

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave")
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Style:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [i18nd("plasma_wallpaper_org.psvec.vlnky", "PSP – wave from RCO"), i18nd("plasma_wallpaper_org.psvec.vlnky", "PS2 – PSX (DESR) wave"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Svec Studio wave")]
        currentIndex: cfg_WaveStyle
        onActivated: cfg_WaveStyle = currentIndex
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        visible: cfg_WaveStyle === 0
        ComboBox {
            id: waveCombo
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 6
            model: waveScanner.waves
            displayText: cfg_WaveName.length > 0 ? currentText : i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom RCO")
            onActivated: {
                cfg_WaveName = waveScanner.waves[currentIndex]
                updateRcoPath()
            }
        }
        Button {
            icon.name: "document-import"
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Import…")
            ToolTip.visible: hovered
            ToolTip.text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Use your own system_plugin_bg.rco file")
            onClicked: importRcoDialog.open()
        }
    }

    Item {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Preview:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        Layout.preferredHeight: root.fieldWidth * 9 / 16
        Layout.maximumHeight: Kirigami.Units.gridUnit * 10
        // A plain Item has implicitHeight 0; the Layout hints alone leave it
        // collapsed in the form layout and clip hides the wave underneath.
        implicitHeight: Math.min(root.fieldWidth * 9 / 16, Kirigami.Units.gridUnit * 10)
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: Kirigami.Units.cornerRadius
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: cfg_ColorTheme > 0 ? previewThemeColor : (cfg_BackgroundMode === 0 ? cfg_BackgroundColor : cfg_GradientStart)
                }
                GradientStop {
                    position: 1.0
                    color: cfg_ColorTheme > 0 ? previewThemeColor : (cfg_BackgroundMode === 0 ? cfg_BackgroundColor : cfg_GradientEnd)
                }
            }
        }

        XmbWaveItem {
            id: configPreview
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: cfg_WaveStyle === 2 ? parent.height : parent.height * 0.85
            rcoPath: cfg_RcoPath.length > 0 ? cfg_RcoPath : ""
            tessLevel: cfg_TessLevel
            maxFps: 15
            paused: true
            pauseOnBattery: false
            batterySaver: cfg_BatterySaver
            waveOpacity: cfg_WaveOpacity
            debugOverlay: cfg_DebugOverlay
            wireframeMode: cfg_WireframeMode
            waveStyle: cfg_WaveStyle
            backgroundMode: cfg_ColorTheme > 0 ? 2 : 0
            backgroundTop: previewThemeColor
            // Style 2 mixes the wave layers into the bottom background colour.
            backgroundBottom: cfg_ColorTheme > 0 ? previewThemeColor
                : (cfg_BackgroundMode === 0 ? cfg_BackgroundColor : cfg_GradientEnd)
            ps2WaveColor: root.waveColorEffective
            waveTint: cfg_WaveColorMode === 1 ? cfg_WaveColor : "white"
            svecWaveColor: root.waveColorEffective
            svecWaveCustom: cfg_WaveColorMode === 1
            renderScale: cfg_RenderScale
            msaaSamples: cfg_MsaaSamples
            bloom: cfg_Bloom
            bicubicTexture: cfg_BicubicTexture
            dither: cfg_Dither
            vignette: cfg_Vignette
            visible: cfg_WaveStyle > 0 || cfg_ColorTheme > 0 || (cfg_RcoPath.length > 0 && (loaded || loading))

            Component.onDestruction: {
                rcoPath = ""
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        type: Kirigami.MessageType.Warning
        visible: root.textureMissing
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "The reflection texture of this wave has not been extracted yet. Copy the command, run it in a terminal and reopen these settings.")
        actions: [
            Kirigami.Action {
                icon.name: "edit-copy"
                text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Copy command")
                onTriggered: {
                    KirigamiPrivate.CopyHelperPrivate.copyTextToClipboard(extractCommand)
                    extractStatus.text = i18nd("plasma_wallpaper_org.psvec.vlnky", "Copied. Open a terminal, paste it and press Enter.")
                    extractStatus.visible = true
                }
            }
        ]
    }

    Kirigami.InlineMessage {
        id: extractStatus
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        type: Kirigami.MessageType.Information
        showCloseButton: true
        visible: false
    }

    Image {
        id: textureProbe
        visible: false
        source: localFileUrl(cfg_TexturePath)
    }

    // ───────────────────────── Animation ─────────────────────────

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Animation")
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Speed:")
        from: 0.1
        to: 4.0
        stepSize: 0.05
        value: cfg_WaveSpeed > 0.05 ? cfg_WaveSpeed : 1.0
        defaultValue: 1.0
        onEdited: v => cfg_WaveSpeed = v
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Height:")
        // The Svec Studio wave fills the whole screen, it has no single band position.
        visible: cfg_WaveStyle !== 2
        from: 0.3
        to: 0.8
        stepSize: 0.02
        value: cfg_WaveHeightRatio > 0.2 ? cfg_WaveHeightRatio : 0.58
        defaultValue: 0.58
        onEdited: v => cfg_WaveHeightRatio = v
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Opacity:")
        from: 0.3
        to: 1.0
        stepSize: 0.05
        value: cfg_WaveOpacity > 0.15 ? cfg_WaveOpacity : 0.92
        defaultValue: 0.92
        onEdited: v => cfg_WaveOpacity = v
    }

    // ───────────────────────── Colours ─────────────────────────

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Colours")
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "XMB colour:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth

        ComboBox {
            id: themeCombo
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 6
            model: root.themeNames
            currentIndex: cfg_ColorTheme
            onActivated: cfg_ColorTheme = currentIndex

            delegate: ItemDelegate {
                required property int index
                required property var modelData
                width: ListView.view ? ListView.view.width : implicitWidth
                highlighted: themeCombo.highlightedIndex === index
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Rectangle {
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        radius: width / 2
                        color: root.themeSwatch(index)
                        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.4)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData
                        font.bold: index === themeCombo.currentIndex
                    }
                }
            }
        }

        Rectangle {
            visible: cfg_ColorTheme > 0
            implicitWidth: Kirigami.Units.gridUnit * 1.5
            implicitHeight: Kirigami.Units.gridUnit * 1.5
            radius: width / 2
            color: previewThemeColor
            border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.4)
        }
    }

    ColorField {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom colour:")
        visible: cfg_ColorTheme === root.customThemeIndex
        value: cfg_CustomThemeColor
        dialogTitle: i18nd("plasma_wallpaper_org.psvec.vlnky", "XMB colour")
        onEdited: c => cfg_CustomThemeColor = c
    }

    CheckBox {
        visible: cfg_ColorTheme > 0
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Dim at night like the console")
        checked: cfg_TimeOfDayBrightness
        onToggled: cfg_TimeOfDayBrightness = checked
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave colour:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth

        ComboBox {
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 6
            model: [i18nd("plasma_wallpaper_org.psvec.vlnky", "Automatic"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom colour")]
            currentIndex: cfg_WaveColorMode
            onActivated: cfg_WaveColorMode = currentIndex
        }

        Rectangle {
            visible: cfg_WaveStyle !== 0
            implicitWidth: Kirigami.Units.gridUnit * 1.5
            implicitHeight: Kirigami.Units.gridUnit * 1.5
            radius: width / 2
            color: root.waveColorEffective
            border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.4)
        }
    }

    ColorField {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom wave:")
        visible: cfg_WaveColorMode === 1
        value: cfg_WaveColor
        dialogTitle: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave colour")
        onEdited: c => cfg_WaveColor = c
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Background:")
        visible: cfg_ColorTheme === 0
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [i18nd("plasma_wallpaper_org.psvec.vlnky", "Solid colour"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Gradient"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Image"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Image from the wave folder")]
        currentIndex: cfg_BackgroundMode
        onActivated: cfg_BackgroundMode = currentIndex
    }

    ComboBox {
        id: bgPresetCombo
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Colour preset:")
        visible: cfg_ColorTheme === 0 && cfg_BackgroundMode <= 1
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: root.backgroundPresets
        textRole: "label"
        displayText: i18nd("plasma_wallpaper_org.psvec.vlnky", "Choose a preset…")
        onActivated: index => {
            const p = root.backgroundPresets[index]
            if (cfg_BackgroundMode === 0) {
                cfg_BackgroundColor = p.bottom
            } else {
                cfg_GradientStart = p.top
                cfg_GradientEnd = p.bottom
            }
        }

        delegate: ItemDelegate {
            required property int index
            required property var modelData
            width: ListView.view ? ListView.view.width : implicitWidth
            highlighted: bgPresetCombo.highlightedIndex === index
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Rectangle {
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.small
                    radius: Kirigami.Units.cornerRadius
                    border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.4)
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: modelData.top
                        }
                        GradientStop {
                            position: 1.0
                            color: modelData.bottom
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: modelData.label
                }
            }
        }
    }

    ColorField {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Colour:")
        visible: cfg_ColorTheme === 0 && cfg_BackgroundMode === 0
        value: cfg_BackgroundColor
        dialogTitle: i18nd("plasma_wallpaper_org.psvec.vlnky", "Background colour")
        onEdited: c => cfg_BackgroundColor = c
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Colours:")
        visible: cfg_ColorTheme === 0 && cfg_BackgroundMode === 1
        spacing: Kirigami.Units.smallSpacing

        ColorField {
            value: cfg_GradientStart
            dialogTitle: i18nd("plasma_wallpaper_org.psvec.vlnky", "Gradient start colour")
            onEdited: c => cfg_GradientStart = c
        }
        ToolButton {
            icon.name: "exchange-positions"
            display: AbstractButton.IconOnly
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Swap colours")
            ToolTip.visible: hovered
            ToolTip.text: text
            onClicked: {
                const s = cfg_GradientStart
                cfg_GradientStart = cfg_GradientEnd
                cfg_GradientEnd = s
            }
        }
        ColorField {
            value: cfg_GradientEnd
            dialogTitle: i18nd("plasma_wallpaper_org.psvec.vlnky", "Gradient end colour")
            onEdited: c => cfg_GradientEnd = c
        }
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Direction:")
        visible: cfg_ColorTheme === 0 && cfg_BackgroundMode === 1
        from: 0
        to: 360
        stepSize: 15
        value: cfg_GradientAngle
        defaultValue: 90
        formatValue: v => Math.round(v) + "°"
        onEdited: v => cfg_GradientAngle = v
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Image:")
        visible: cfg_ColorTheme === 0 && cfg_BackgroundMode === 2
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        TextField {
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 6
            text: String(cfg_BackgroundImage).replace("file://", "")
            placeholderText: i18nd("plasma_wallpaper_org.psvec.vlnky", "No image selected")
            onEditingFinished: cfg_BackgroundImage = text.length > 0 && !text.startsWith("file://") ? "file://" + text : text
        }
        Button {
            icon.name: "document-open"
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Browse…")
            onClicked: imageDialog.open()
        }
    }

    // ───────────────────────── Quality & power ─────────────────────────

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Quality")
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Preset:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: root.qualityPresets.map(p => p.label).concat([i18nd("plasma_wallpaper_org.psvec.vlnky", "Custom")])
        currentIndex: root.currentQualityPreset
        onActivated: index => {
            if (index < root.qualityPresets.length) {
                const p = root.qualityPresets[index]
                applyPreset(p.scale, p.msaa, p.bloom, p.tess, p.fps)
            } else {
                root.showAdvanced = true
            }
        }
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Max FPS:")
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [
            {
                label: "15",
                value: 15
            },
            {
                label: "30",
                value: 30
            },
            {
                label: "60",
                value: 60
            },
            {
                label: "120",
                value: 120
            },
            {
                label: "144",
                value: 144
            }
        ]
        textRole: "label"
        valueRole: "value"
        currentIndex: cfg_MaxFps <= 15 ? 0 : (cfg_MaxFps <= 30 ? 1 : (cfg_MaxFps <= 60 ? 2 : (cfg_MaxFps <= 120 ? 3 : 4)))
        onActivated: cfg_MaxFps = model[currentIndex].value
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Glow:")
        from: 0.0
        to: 1.0
        stepSize: 0.05
        value: cfg_Bloom
        defaultValue: 0.3
        formatValue: v => v < 0.01 ? i18nd("plasma_wallpaper_org.psvec.vlnky", "off") : Math.round(v * 100) + " %"
        onEdited: v => cfg_Bloom = v
    }

    ValueSlider {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Vignette:")
        from: 0.0
        to: 0.6
        stepSize: 0.05
        value: cfg_Vignette
        defaultValue: 0.0
        formatValue: v => v < 0.01 ? i18nd("plasma_wallpaper_org.psvec.vlnky", "off") : Math.round(v * 100) + " %"
        onEdited: v => cfg_Vignette = v
    }

    CheckBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "On battery:")
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Pause the animation")
        checked: cfg_PauseOnBattery
        onToggled: cfg_PauseOnBattery = checked
    }

    CheckBox {
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Battery saver (15 FPS, lower quality)")
        checked: cfg_BatterySaver
        onToggled: cfg_BatterySaver = checked
    }

    // ───────────────────────── Advanced ─────────────────────────

    Kirigami.Separator {
        Kirigami.FormData.isSection: true
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Advanced")
    }

    CheckBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Show:")
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Advanced settings")
        checked: root.showAdvanced
        onToggled: root.showAdvanced = checked
    }

    RowLayout {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wave collection:")
        visible: root.showAdvanced || waveScanner.waves.length === 0
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        TextField {
            id: collectionField
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 6
            text: cfg_CollectionPath
            placeholderText: defaultCollectionPath
            onEditingFinished: {
                cfg_CollectionPath = text.trim()
                waveScanner.collectionPath = cfg_CollectionPath
                updateRcoPath()
            }
        }
        Button {
            icon.name: "document-open-folder"
            text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Browse…")
            onClicked: collectionDialog.open()
        }
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Mesh detail:")
        visible: root.showAdvanced
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [i18nd("plasma_wallpaper_org.psvec.vlnky", "Low"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Medium"), i18nd("plasma_wallpaper_org.psvec.vlnky", "High"), i18nd("plasma_wallpaper_org.psvec.vlnky", "Ultra (1440p / 4K)")]
        currentIndex: cfg_TessLevel - 1
        onActivated: cfg_TessLevel = currentIndex + 1
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Supersampling:")
        visible: root.showAdvanced
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [
            {
                label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Off (1×)"),
                value: 1.0
            },
            {
                label: "1.5×",
                value: 1.5
            },
            {
                label: "2× (SSAA)",
                value: 2.0
            }
        ]
        textRole: "label"
        valueRole: "value"
        currentIndex: cfg_RenderScale >= 1.9 ? 2 : (cfg_RenderScale >= 1.4 ? 1 : 0)
        onActivated: cfg_RenderScale = model[currentIndex].value
    }

    ComboBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Antialiasing:")
        visible: root.showAdvanced
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        model: [
            {
                label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Off"),
                value: 1
            },
            {
                label: "MSAA 2×",
                value: 2
            },
            {
                label: "MSAA 4×",
                value: 4
            },
            {
                label: "MSAA 8×",
                value: 8
            }
        ]
        textRole: "label"
        valueRole: "value"
        currentIndex: cfg_MsaaSamples >= 8 ? 3 : (cfg_MsaaSamples >= 4 ? 2 : (cfg_MsaaSamples >= 2 ? 1 : 0))
        onActivated: cfg_MsaaSamples = model[currentIndex].value
    }

    CheckBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Rendering:")
        visible: root.showAdvanced
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Smooth (bicubic) reflection texture")
        checked: cfg_BicubicTexture
        onToggled: cfg_BicubicTexture = checked
    }

    CheckBox {
        visible: root.showAdvanced
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Dithering (no colour banding)")
        checked: cfg_Dither
        onToggled: cfg_Dither = checked
    }

    CheckBox {
        visible: root.showAdvanced
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Adaptive quality (auto mesh detail)")
        checked: cfg_AdaptiveQuality
        onToggled: cfg_AdaptiveQuality = checked
    }

    Button {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Textures:")
        visible: root.showAdvanced
        icon.name: "edit-copy"
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Copy extract-all command")
        ToolTip.visible: hovered
        ToolTip.text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Extracts reflection textures of all waves in the collection")
        onClicked: {
            const cmd = "bash \"" + extractAllScript + "\" \"" + cfg_CollectionPath.replace("file://", "") + "\""
            KirigamiPrivate.CopyHelperPrivate.copyTextToClipboard(cmd)
            extractStatus.text = i18nd("plasma_wallpaper_org.psvec.vlnky", "Copied extract-all command (all waves).")
            extractStatus.visible = true
        }
    }

    CheckBox {
        Kirigami.FormData.label: i18nd("plasma_wallpaper_org.psvec.vlnky", "Diagnostics:")
        visible: root.showAdvanced
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Show debug overlay")
        checked: cfg_DebugOverlay
        onToggled: cfg_DebugOverlay = checked
    }

    CheckBox {
        visible: root.showAdvanced
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Wireframe mesh")
        checked: cfg_WireframeMode
        onToggled: cfg_WireframeMode = checked
    }

    Button {
        visible: root.showAdvanced
        icon.name: "camera-photo"
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "Capture debug screenshots (F12)")
        enabled: cfg_RcoPath.length > 0
        onClicked: configPreview.captureAllDebugScreenshots()
    }

    Label {
        visible: root.showAdvanced && cfg_RcoPath.length > 0
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        text: i18nd("plasma_wallpaper_org.psvec.vlnky", "RCO: %1", cfg_RcoPath)
        wrapMode: Text.WrapAnywhere
        opacity: 0.7
        font: Kirigami.Theme.smallFont
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        visible: root.showAdvanced && cfg_DebugOverlay && configPreview.diagnostics.length > 0
        text: configPreview.diagnostics
    }

    FolderDialog {
        id: collectionDialog
        title: i18nd("plasma_wallpaper_org.psvec.vlnky", "Select wave collection folder")
        currentFolder: collectionFolderUrl()
        onAccepted: {
            cfg_CollectionPath = selectedFolder.toString().replace("file://", "")
            collectionField.text = cfg_CollectionPath
            waveScanner.collectionPath = cfg_CollectionPath
            updateRcoPath()
        }
    }

    FileDialog {
        id: imageDialog
        title: i18nd("plasma_wallpaper_org.psvec.vlnky", "Background image")
        nameFilters: ["Images (*.png *.jpg *.jpeg *.bmp *.webp)"]
        onAccepted: cfg_BackgroundImage = selectedFile
    }

    FileDialog {
        id: importRcoDialog
        title: i18nd("plasma_wallpaper_org.psvec.vlnky", "Import wave RCO")
        nameFilters: ["RCO (*.rco)"]
        onAccepted: {
            const src = selectedFile.toString().replace("file://", "").trim()
            if (src.length < 3)
                return
            cfg_RcoPath = src
            cfg_TexturePath = ""
            cfg_WaveName = ""
        }
    }
}

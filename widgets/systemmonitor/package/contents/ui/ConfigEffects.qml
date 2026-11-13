// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: page
    title: qsTr("Liquid Glass")
    property alias cfg_opticsEnabled: optics.checked
    property alias cfg_refraction: refraction.value
    property alias cfg_dispersion: dispersion.value
    property alias cfg_specular: specular.value
    property alias cfg_surfaceHoverEnabled: surfaceHover.checked
    property alias cfg_hoverGlow: hoverGlow.value
    property alias cfg_blurRadius: blur.value
    property alias cfg_materialOpacity: materialOpacity.value
    property alias cfg_tintColor: tintColor.color
    property alias cfg_tintStrength: tint.value
    property alias cfg_adaptiveContrast: contrast.value
    property alias cfg_cornerRadius: corners.value

    function restoreDefaults() {
        cfg_opticsEnabled = true
        cfg_refraction = 100
        cfg_dispersion = 25
        cfg_specular = 100
        cfg_surfaceHoverEnabled = true
        cfg_hoverGlow = 0
        cfg_blurRadius = 20
        cfg_materialOpacity = 40
        cfg_tintColor = "#296b75"
        cfg_tintStrength = 20
        cfg_adaptiveContrast = 100
        cfg_cornerRadius = 20
    }

    implicitWidth: 620
    implicitHeight: 660
    topPadding: 16
    bottomPadding: 16
    leftPadding: 16
    rightPadding: 16
    clip: true

    ColumnLayout {
        objectName: "settingsContents"
        width: parent.width
        spacing: 16

        Controls.Label { text: qsTr("Material preview"); font.bold: true }
        Rectangle {
            id: previewBackground
            Layout.fillWidth: true
            Layout.preferredHeight: 148
            clip: true
            // The preview source contains only this backdrop, never the glass.
            Item {
                id: sampleWallpaper
                anchors.fill: parent
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#123857" }
                        GradientStop { position: 0.48; color: "#7accc5" }
                        GradientStop { position: 1; color: "#dfb86c" }
                    }
                }
                Rectangle { x: parent.width * 0.28; y: -30; width: 70; height: 220; rotation: 28; color: "#d1e9ee" }
                Rectangle { x: parent.width * 0.75; y: 35; width: 100; height: 100; radius: 50; color: "#af705d" }
            }
            GlassCard {
                id: preview
                objectName: "effectsPreview"
                anchors.fill: parent
                anchors.margins: 18
                wallpaper: sampleWallpaper
                opticsEnabled: page.cfg_opticsEnabled
                refraction: page.cfg_refraction / 100
                dispersion: page.cfg_dispersion / 100
                specular: page.cfg_specular / 100
                surfaceHoverEnabled: page.cfg_surfaceHoverEnabled
                hoverGlow: page.cfg_hoverGlow / 100
                blurRadius: page.cfg_blurRadius / 10
                materialOpacity: page.cfg_materialOpacity / 100
                tintColor: page.cfg_tintColor
                tintStrength: page.cfg_tintStrength / 100
                adaptiveContrast: page.cfg_adaptiveContrast / 100
                cornerRadius: page.cfg_cornerRadius

                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    Text { text: "EMERALD GLASS"; color: "#e5f3f8"; font.family: "sans-serif"; font.pixelSize: 11; font.letterSpacing: 2 }
                    Text { text: "42.0 °C"; color: "#f5fcff"; font.family: "sans-serif"; font.pixelSize: 27; font.weight: Font.DemiBold }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            ColumnLayout {
                Layout.columnSpan: 2
                spacing: 0
                Controls.CheckBox {
                    id: optics
                    text: qsTr("Liquid Glass optics")
                    checked: true
                }
                Controls.CheckBox {
                    id: surfaceHover
                    text: qsTr("Cursor glow across the surface")
                    enabled: optics.checked
                    checked: true
                }
            }
            Controls.Label {
                id: refractionLabel
                text: qsTr("Refraction:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: refraction
                objectName: "refractionControl"
                accessibleName: refractionLabel.text
                enabled: optics.checked
                to: 180
                value: 100
            }
            Controls.Label {
                id: dispersionLabel
                text: qsTr("Dispersion:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: dispersion
                accessibleName: dispersionLabel.text
                enabled: optics.checked
                value: 25
            }
            Controls.Label {
                id: specularLabel
                text: qsTr("Edge highlights:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: specular
                accessibleName: specularLabel.text
                enabled: optics.checked
                to: 150
                value: 100
            }
            Controls.Label {
                id: hoverGlowLabel
                text: qsTr("Cursor glow:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: hoverGlow
                accessibleName: hoverGlowLabel.text
                enabled: optics.checked
                to: 150
                value: 0
            }
            Controls.Label {
                id: blurLabel
                text: qsTr("Blur:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: blur
                accessibleName: blurLabel.text
                to: 100
                value: 20
            }
            Controls.Label {
                id: materialOpacityLabel
                text: qsTr("Material opacity:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: materialOpacity
                accessibleName: materialOpacityLabel.text
                value: 40
            }
            Controls.Label {
                id: tintColorLabel
                text: qsTr("Tint color:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            KQuickControls.ColorButton {
                id: tintColor
                color: "#296b75"
                showAlphaChannel: false
            }
            Controls.Label {
                id: tintLabel
                text: qsTr("Tint strength:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: tint
                accessibleName: tintLabel.text
                value: 20
            }
            Controls.Label {
                id: contrastLabel
                text: qsTr("Adaptive contrast:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: contrast
                accessibleName: contrastLabel.text
                value: 100
            }
            Controls.Label {
                id: cornersLabel
                text: qsTr("Corner radius:")
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
            EffectSlider {
                id: corners
                accessibleName: cornersLabel.text
                to: 40
                value: 20
                suffix: qsTr("px")
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Changes appear in the preview immediately. Apply saves them to this widget. Material opacity affects the background; text and graphs remain opaque.")
        }
        Controls.Button {
            text: qsTr("Restore first-version appearance")
            icon.name: "edit-reset"
            onClicked: page.restoreDefaults()
        }
    }
}

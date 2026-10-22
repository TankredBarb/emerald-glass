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

        Kirigami.FormLayout {
            Layout.fillWidth: true
            Controls.CheckBox {
                id: optics
                text: qsTr("Liquid Glass optics")
                checked: true
            }
            EffectSlider {
                id: refraction
                Kirigami.FormData.label: qsTr("Refraction:")
                accessibleName: Kirigami.FormData.label
                enabled: optics.checked
                to: 180
                value: 100
            }
            EffectSlider {
                id: dispersion
                Kirigami.FormData.label: qsTr("Dispersion:")
                accessibleName: Kirigami.FormData.label
                enabled: optics.checked
                value: 25
            }
            EffectSlider {
                id: specular
                Kirigami.FormData.label: qsTr("Edge highlights:")
                accessibleName: Kirigami.FormData.label
                enabled: optics.checked
                to: 150
                value: 100
            }
            EffectSlider {
                id: hoverGlow
                Kirigami.FormData.label: qsTr("Cursor glow:")
                accessibleName: Kirigami.FormData.label
                enabled: optics.checked
                to: 150
                value: 0
            }
            EffectSlider {
                id: blur
                Kirigami.FormData.label: qsTr("Blur:")
                accessibleName: Kirigami.FormData.label
                to: 100
                value: 20
            }
            EffectSlider {
                id: materialOpacity
                Kirigami.FormData.label: qsTr("Material opacity:")
                accessibleName: Kirigami.FormData.label
                value: 40
            }
            KQuickControls.ColorButton {
                id: tintColor
                Kirigami.FormData.label: qsTr("Tint color:")
                color: "#296b75"
                showAlphaChannel: false
            }
            EffectSlider {
                id: tint
                Kirigami.FormData.label: qsTr("Tint strength:")
                accessibleName: Kirigami.FormData.label
                value: 20
            }
            EffectSlider {
                id: contrast
                Kirigami.FormData.label: qsTr("Adaptive contrast:")
                accessibleName: Kirigami.FormData.label
                value: 100
            }
            EffectSlider {
                id: corners
                Kirigami.FormData.label: qsTr("Corner radius:")
                accessibleName: Kirigami.FormData.label
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

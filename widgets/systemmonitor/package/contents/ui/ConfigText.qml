// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: page
    title: qsTr("Text and charts")
    property alias cfg_valueFont: valueFont.selectedFont
    property alias cfg_titleFont: titleFont.selectedFont
    property alias cfg_detailFont: detailFont.selectedFont
    property alias cfg_unifiedTextColor: unifiedText.checked
    property alias cfg_valueColor: valueColor.color
    property alias cfg_titleColor: titleColor.color
    property alias cfg_detailColor: detailColor.color
    property alias cfg_cpuGraphColor: cpuGraphColor.color
    property alias cfg_gpuTemperatureGraphColor: gpuGraphColor.color
    property alias cfg_cpuTemperatureGraphColor: cpuTemperatureGraphColor.color
    property alias cfg_chartOutline: chartOutline.checked

    function restoreDefaults() {
        cfg_unifiedTextColor = true
        cfg_valueFont = Qt.font({family: "sans-serif", pixelSize: 27, weight: Font.DemiBold})
        cfg_titleFont = Qt.font({family: "sans-serif", pixelSize: 12, weight: Font.DemiBold})
        cfg_detailFont = Qt.font({family: "sans-serif", pixelSize: 10})
        cfg_valueColor = "#f5fcff"
        cfg_titleColor = "#e5f3f8"
        cfg_detailColor = "#b5c9d2"
    }

    function restoreChartDefaults() {
        cfg_cpuGraphColor = "#63e5ef"
        cfg_gpuTemperatureGraphColor = "#ffd27a"
        cfg_cpuTemperatureGraphColor = "#f6a0cf"
        cfg_chartOutline = true
    }

    implicitWidth: 620
    implicitHeight: 600
    topPadding: 16
    bottomPadding: 16
    leftPadding: 16
    rightPadding: 16

    ColumnLayout {
        width: parent.width
        spacing: 16
        Controls.Label { text: qsTr("Text preview"); font.bold: true }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(130, preview.implicitHeight + 40)
            radius: 20
            color: "#20394a"
            SensorText {
                id: preview
                objectName: "textPreview"
                anchors.fill: parent
                anchors.margins: 20
                title: "CPU"
                valueText: "42.0 °C"
                detail: qsTr("Temperature")
                accent: "#63e5ef"
                valueFont: page.cfg_valueFont
                titleFont: page.cfg_titleFont
                detailFont: page.cfg_detailFont
                valueColor: page.cfg_valueColor
                titleColor: page.cfg_unifiedTextColor ? page.cfg_valueColor : page.cfg_titleColor
                detailColor: page.cfg_unifiedTextColor ? page.cfg_valueColor : page.cfg_detailColor
            }
        }
        Controls.CheckBox {
            id: unifiedText
            text: qsTr("Use value color for all text and dividers")
            checked: true
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            TextFontButton {
                id: valueFont
                objectName: "valueFontButton"
                Kirigami.FormData.label: qsTr("Values font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 27, weight: Font.DemiBold})
            }
            KQuickControls.ColorButton {
                id: valueColor
                Kirigami.FormData.label: qsTr("Values color:")
                color: "#f5fcff"
                showAlphaChannel: false
            }
            TextFontButton {
                id: titleFont
                Kirigami.FormData.label: qsTr("Headings font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 12, weight: Font.DemiBold})
            }
            KQuickControls.ColorButton {
                id: titleColor
                enabled: !page.cfg_unifiedTextColor
                Kirigami.FormData.label: qsTr("Headings color:")
                color: "#e5f3f8"
                showAlphaChannel: false
            }
            TextFontButton {
                id: detailFont
                Kirigami.FormData.label: qsTr("Descriptions font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 10})
            }
            KQuickControls.ColorButton {
                id: detailColor
                enabled: !page.cfg_unifiedTextColor
                Kirigami.FormData.label: qsTr("Descriptions color:")
                color: "#b5c9d2"
                showAlphaChannel: false
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Choose the font, size and style in the font dialog. Apply updates this widget; glass settings are kept separately.")
        }
        Controls.Button {
            text: qsTr("Restore original text")
            icon.name: "edit-reset"
            onClicked: page.restoreDefaults()
        }
        Controls.Label { text: qsTr("Charts on transparent glass"); font.bold: true }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            KQuickControls.ColorButton {
                id: cpuGraphColor
                Kirigami.FormData.label: qsTr("CPU load:")
                color: "#63e5ef"
                showAlphaChannel: false
            }
            KQuickControls.ColorButton {
                id: gpuGraphColor
                Kirigami.FormData.label: qsTr("GPU temperature:")
                color: "#ffd27a"
                showAlphaChannel: false
            }
            KQuickControls.ColorButton {
                id: cpuTemperatureGraphColor
                Kirigami.FormData.label: qsTr("CPU temperature:")
                color: "#f6a0cf"
                showAlphaChannel: false
            }
            Controls.CheckBox {
                id: chartOutline
                text: qsTr("Contrast outline")
                checked: true
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("A thin contrasting outline follows the line color, keeping charts visible on wallpaper with material opacity and blur at zero.")
        }
        Controls.Button {
            text: qsTr("Restore original chart colors")
            icon.name: "edit-reset"
            onClicked: page.restoreChartDefaults()
        }
    }
}

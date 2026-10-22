// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQuickControls
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: page
    title: qsTr("Text")
    property alias cfg_valueFont: valueFont.selectedFont
    property alias cfg_titleFont: titleFont.selectedFont
    property alias cfg_detailFont: detailFont.selectedFont
    property alias cfg_valueColor: valueColor.color
    property alias cfg_titleColor: titleColor.color
    property alias cfg_detailColor: detailColor.color

    function restoreDefaults() {
        cfg_valueFont = Qt.font({family: "sans-serif", pixelSize: 44, weight: Font.DemiBold})
        cfg_titleFont = Qt.font({family: "sans-serif", pixelSize: 16, weight: Font.DemiBold})
        cfg_detailFont = Qt.font({family: "sans-serif", pixelSize: 12})
        cfg_valueColor = "#f5fcff"
        cfg_titleColor = "#e5f3f8"
        cfg_detailColor = "#b5c9d2"
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
                title: "Одесса"
                valueText: "18.0 °C"
                detail: qsTr("Temperature")
                accent: "#63e5ef"
                valueFont: page.cfg_valueFont
                titleFont: page.cfg_titleFont
                detailFont: page.cfg_detailFont
                valueColor: page.cfg_valueColor
                titleColor: page.cfg_titleColor
                detailColor: page.cfg_detailColor
            }
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            TextFontButton {
                id: valueFont
                objectName: "valueFontButton"
                Kirigami.FormData.label: qsTr("Temperature font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 44, weight: Font.DemiBold})
            }
            KQuickControls.ColorButton {
                id: valueColor
                Kirigami.FormData.label: qsTr("Temperature color:")
                color: "#f5fcff"
                showAlphaChannel: false
            }
            TextFontButton {
                id: titleFont
                Kirigami.FormData.label: qsTr("City font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 16, weight: Font.DemiBold})
            }
            KQuickControls.ColorButton {
                id: titleColor
                Kirigami.FormData.label: qsTr("City color:")
                color: "#e5f3f8"
                showAlphaChannel: false
            }
            TextFontButton {
                id: detailFont
                Kirigami.FormData.label: qsTr("Details and forecast font:")
                selectedFont: Qt.font({family: "sans-serif", pixelSize: 12})
            }
            KQuickControls.ColorButton {
                id: detailColor
                Kirigami.FormData.label: qsTr("Details and forecast color:")
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
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: textBlock
    property string title
    property string valueText
    property string detail
    property color accent
    property font valueFont: Qt.font({family: "sans-serif", pixelSize: 27, weight: Font.DemiBold})
    property font titleFont: Qt.font({family: "sans-serif", pixelSize: 12, weight: Font.DemiBold})
    property font detailFont: Qt.font({family: "sans-serif", pixelSize: 10})
    property color valueColor: "#f5fcff"
    property color titleColor: "#e5f3f8"
    property color detailColor: "#b5c9d2"
    spacing: 3

    RowLayout {
        Layout.fillWidth: true
        spacing: 5
        Rectangle { implicitWidth: 5; implicitHeight: 5; radius: 2.5; color: textBlock.accent }
        Text {
            renderType: Text.QtRendering
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: textBlock.title
            color: textBlock.titleColor
            font: textBlock.titleFont
            elide: Text.ElideRight
        }
    }
    Text {
        renderType: Text.QtRendering
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: textBlock.valueText
        color: textBlock.valueColor
        font: textBlock.valueFont
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 8
        elide: Text.ElideRight
    }
    Text {
        renderType: Text.QtRendering
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: textBlock.detail
        color: textBlock.detailColor
        font: textBlock.detailFont
        elide: Text.ElideRight
    }
}

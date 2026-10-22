// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs

Controls.Button {
    id: control
    property font selectedFont
    text: selectedFont.family + ", " + qsTr("%1 pt").arg(fontInfo.pointSize)
    onClicked: {
        dialog.selectedFont = Qt.font({
            family: selectedFont.family,
            styleName: selectedFont.styleName,
            pointSize: fontInfo.pointSize,
            weight: selectedFont.weight,
            italic: selectedFont.italic,
            underline: selectedFont.underline,
            strikeout: selectedFont.strikeout
        })
        dialog.open()
    }

    FontInfo { id: fontInfo; font: control.selectedFont }

    FontDialog {
        id: dialog
        objectName: "fontDialog"
        title: qsTr("Choose font")
        onAccepted: control.selectedFont = selectedFont
    }
}

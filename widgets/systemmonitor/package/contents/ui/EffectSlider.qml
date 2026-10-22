// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

RowLayout {
    id: control
    Layout.fillWidth: true
    property alias value: spin.value
    property alias from: spin.from
    property alias to: spin.to
    property string suffix: "%"
    property string accessibleName
    spacing: 8

    Controls.Slider {
        Layout.fillWidth: true
        Layout.minimumWidth: 100
        from: spin.from
        to: spin.to
        stepSize: 1
        value: spin.value
        onMoved: spin.value = Math.round(value)
        Accessible.name: control.accessibleName
    }
    Controls.SpinBox {
        id: spin
        from: 0
        to: 100
        editable: true
        Accessible.name: control.accessibleName
    }
    Controls.Label {
        Layout.preferredWidth: 20
        text: control.suffix
    }
}

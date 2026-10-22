// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils
import org.kde.ksysguard.faces as Faces
import "SensorLabels.js" as SensorLabels

KCMUtils.SimpleKCM {
    id: page
    title: qsTr("Sensors")
    property alias cfg_sensorIds: selection.selected
    property alias cfg_sensorColumns: columns.value
    property string cfg_sensorLabels: "{}"
    implicitWidth: 620
    implicitHeight: 600
    topPadding: 16
    bottomPadding: 16
    leftPadding: 16
    rightPadding: 16
    ColumnLayout {
        width: parent.width
        spacing: 16
        Kirigami.FormLayout {
            Layout.fillWidth: true
            Controls.SpinBox {
                id: columns
                objectName: "sensorColumns"
                Kirigami.FormData.label: qsTr("Columns:")
                from: 0
                to: 12
                value: 3
                textFromValue: value => value === 0 ? qsTr("Single row") : value.toString()
            }
        }
        Controls.Label { text: qsTr("Displayed sensors"); font.bold: true }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Click the selection field to browse or search KDE sensors. Drag selected sensors to change their order; use the remove button to delete one. Use the pencil to rename a sensor, then press Enter or the checkmark. An empty name restores its default title. Apply saves the selection and names for this widget.")
        }
        Faces.Choices {
            id: selection
            objectName: "sensorSelection"
            Layout.fillWidth: true
            supportsColors: false
            labelsEditable: true
            labels: SensorLabels.parse(page.cfg_sensorLabels)
            onSensorLabelChanged: (sensorId, label) => {
                const names = SensorLabels.parse(page.cfg_sensorLabels)
                if (label.trim().length > 0) names[sensorId] = label.trim()
                else delete names[sensorId]
                page.cfg_sensorLabels = JSON.stringify(names)
            }
            selected: ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]
        }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Availability depends on the hardware and KDE sensor service. Unavailable sensors remain selected and display a dash. Choose the number of columns for a grid, or Single row to keep everything in one line. Narrow layouts scroll horizontally.")
        }
        Controls.Button {
            text: qsTr("Restore initial sensors")
            icon.name: "edit-reset"
            onClicked: page.cfg_sensorIds = ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]
        }
    }
}

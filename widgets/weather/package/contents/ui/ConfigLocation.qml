// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils
import "backend" as Backend

KCMUtils.SimpleKCM {
    id: page
    title: qsTr("Location and provider")
    property alias cfg_provider: provider.currentIndex
    property string cfg_locationName: "Одесса"
    property double cfg_latitude: 46.48572
    property double cfg_longitude: 30.74383
    property alias cfg_timeZone: timeZone.text
    property alias cfg_refreshMinutes: refresh.value
    topPadding: 16
    bottomPadding: 16
    leftPadding: 16
    rightPadding: 16
    implicitWidth: 620
    implicitHeight: 620

    Backend.WeatherBackend { id: search }
    ColumnLayout {
        width: parent.width
        spacing: 16
        Kirigami.FormLayout {
            Layout.fillWidth: true
            Controls.ComboBox {
                id: provider
                Kirigami.FormData.label: qsTr("Weather provider:")
                model: ["Open-Meteo", "MET Norway"]
            }
            Controls.Label {
                Kirigami.FormData.label: qsTr("Selected location:")
                text: page.cfg_locationName + "\n" + page.cfg_latitude.toFixed(4) + ", " + page.cfg_longitude.toFixed(4)
            }
            Controls.SpinBox {
                id: refresh
                Kirigami.FormData.label: qsTr("Refresh interval (min):")
                from: 15
                to: 120
                stepSize: 15
                value: 30
            }
        }
        Controls.Label { text: qsTr("Search any city, town or village"); font.bold: true }
        RowLayout {
            Layout.fillWidth: true
            Controls.TextField {
                id: query
                Layout.fillWidth: true
                placeholderText: qsTr("For example: Одеса, Ізмаїл, Chișinău…")
                onAccepted: search.searchLocations(text)
            }
            Controls.Button {
                text: qsTr("Search")
                enabled: !search.searching
                onClicked: search.searchLocations(query.text)
            }
            Controls.BusyIndicator { implicitWidth: 24; implicitHeight: 24; running: search.searching; visible: running }
        }
        Controls.Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: search.searchError.length > 0; text: search.searchError }
        ListView {
            id: choices
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 200)
            clip: true
            model: search.locations
            delegate: Controls.ItemDelegate {
                id: choice
                required property var modelData
                width: choices.width
                text: modelData.label
                onClicked: {
                    page.cfg_locationName = modelData.name
                    page.cfg_latitude = modelData.latitude
                    page.cfg_longitude = modelData.longitude
                    if (modelData.timeZone) page.cfg_timeZone = modelData.timeZone
                }
            }
            Controls.ScrollBar.vertical: Controls.ScrollBar {}
        }
        Controls.CheckBox { id: manual; text: qsTr("Edit name and coordinates manually") }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: manual.checked
            Controls.TextField {
                Kirigami.FormData.label: qsTr("Display name:")
                text: page.cfg_locationName
                onTextEdited: page.cfg_locationName = text
            }
            Controls.TextField {
                Kirigami.FormData.label: qsTr("Latitude:")
                text: page.cfg_latitude.toString()
                validator: DoubleValidator { bottom: -90; top: 90; decimals: 6; locale: "C" }
                onEditingFinished: {
                    if (acceptableInput) page.cfg_latitude = Number.fromLocaleString(Qt.locale("C"), text)
                    text = Qt.binding(() => page.cfg_latitude.toString())
                }
            }
            Controls.TextField {
                Kirigami.FormData.label: qsTr("Longitude:")
                text: page.cfg_longitude.toString()
                validator: DoubleValidator { bottom: -180; top: 180; decimals: 6; locale: "C" }
                onEditingFinished: {
                    if (acceptableInput) page.cfg_longitude = Number.fromLocaleString(Qt.locale("C"), text)
                    text = Qt.binding(() => page.cfg_longitude.toString())
                }
            }
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            Controls.TextField {
                id: timeZone
                Kirigami.FormData.label: qsTr("Time zone:")
                text: "Europe/Kyiv"
                placeholderText: "Europe/Kyiv"
            }
        }
        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Both weather providers use coordinates worldwide. Location search: Open-Meteo / GeoNames (CC BY 4.0). If a village is missing, enter its coordinates manually.")
        }
    }
}

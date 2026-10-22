// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: card
    function showForecast() { forecastPopup.open() }
    property var weather
    property Item wallpaper
    property string cityName: "Одесса"
    property alias material: glass
    property font valueFont: Qt.font({family: "sans-serif", pixelSize: 44, weight: Font.DemiBold})
    property font titleFont: Qt.font({family: "sans-serif", pixelSize: 16, weight: Font.DemiBold})
    property font detailFont: Qt.font({family: "sans-serif", pixelSize: 12})
    property color valueColor: "#f5fcff"
    property color titleColor: "#e5f3f8"
    property color detailColor: "#b5c9d2"
    readonly property var current: weather?.current ?? ({})
    readonly property real minimumCardHeight: Math.max(320, content.implicitHeight + 40)
    implicitWidth: 496
    implicitHeight: Math.max(360, minimumCardHeight)

    GlassCard { id: glass; anchors.fill: parent; wallpaper: card.wallpaper }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: point => {
            if (point.position.y > content.y + toolbar.y + toolbar.height
                && point.position.y < content.y + attribution.y) forecastPopup.open()
        }
    }
    ForecastPopup {
        id: forecastPopup
        weather: card.weather
        cityName: card.cityName
        wallpaper: card.wallpaper
        material: glass
        titleFont: card.titleFont
        detailFont: card.detailFont
        valueColor: card.valueColor
        titleColor: card.titleColor
        detailColor: card.detailColor
    }
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: card.cityName
                color: card.titleColor
                font: card.titleFont
                elide: Text.ElideRight
            }
            Controls.ToolButton {
                objectName: "openForecast"
                icon.name: "view-calendar"
                icon.color: card.detailColor
                onClicked: forecastPopup.open()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Extended forecast")
                Accessible.name: qsTr("Extended forecast")
            }
            Controls.BusyIndicator { implicitWidth: 20; implicitHeight: 20; running: card.weather?.loading ?? false; visible: running }
            Controls.ToolButton {
                icon.name: "view-refresh"
                icon.color: card.detailColor
                text: qsTr("Refresh")
                display: Controls.AbstractButton.IconOnly
                enabled: card.weather?.canRefresh ?? false
                onClicked: card.weather.refresh()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: enabled ? qsTr("Refresh") : qsTr("Next refresh after %1").arg(card.weather?.nextRefreshText ?? "")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            visible: card.weather?.hasData ?? false
            Kirigami.Icon {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                source: card.current.icon ?? "weather-none-available"
                isMask: true
                color: card.valueColor
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    objectName: "temperature"
                    text: card.current.temperatureText ?? "—"
                    font: card.valueFont
                    color: card.valueColor
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 8
                }
                Text { text: card.current.condition ?? ""; font: card.detailFont; color: card.titleColor }
                Text {
                    visible: (card.current.feelsText ?? "").length > 0
                    text: qsTr("Feels like %1").arg(card.current.feelsText ?? "—")
                    font: card.detailFont
                    color: card.detailColor
                }
            }
            ColumnLayout {
                spacing: 5
                Text { text: qsTr("Wind  %1").arg(card.current.windText ?? "—"); font: card.detailFont; color: card.detailColor }
                Text { text: qsTr("Humidity  %1").arg(card.current.humidityText ?? "—"); font: card.detailFont; color: card.detailColor }
                Text { text: card.current.pressureText ?? "—"; font: card.detailFont; color: card.detailColor }
            }
        }
        Text {
            Layout.fillWidth: true
            objectName: "loadingMessage"
            visible: !(card.weather?.hasData ?? false)
            text: card.weather?.loading ? qsTr("Loading forecast…") : (card.weather?.error || qsTr("Choose a location in the widget settings."))
            wrapMode: Text.WordWrap
            font: card.detailFont
            color: card.detailColor
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: card.detailColor; opacity: 0.2; visible: card.weather?.hasData ?? false }
        RowLayout {
            Layout.fillWidth: true
            objectName: "hours"
            visible: (card.weather?.hours.length ?? 0) > 0
            spacing: 10
            Repeater {
                model: card.weather?.hours ?? []
                delegate: Item {
                    id: hour
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: children[0].implicitHeight
                    ColumnLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 3
                    Text { Layout.alignment: Qt.AlignHCenter; text: hour.modelData.timeText; font: card.detailFont; color: card.detailColor }
                    Kirigami.Icon { Layout.alignment: Qt.AlignHCenter; implicitWidth: 22; implicitHeight: 22; source: hour.modelData.icon; isMask: true; color: card.valueColor }
                    Text { Layout.alignment: Qt.AlignHCenter; text: hour.modelData.temperatureText; font: card.detailFont; color: card.valueColor }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: card.detailColor; opacity: 0.2; visible: (card.weather?.days.length ?? 0) > 0 }
        RowLayout {
            Layout.fillWidth: true
            visible: (card.weather?.days.length ?? 0) > 0
            spacing: 10
            Repeater {
                objectName: "compactDaysModel"
                model: (card.weather?.days ?? []).slice(0, 4)
                delegate: Item {
                    id: day
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: children[0].implicitHeight
                    ColumnLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 3
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Text { text: day.modelData.dayText; font: card.detailFont; color: card.titleColor }
                        Kirigami.Icon { implicitWidth: 22; implicitHeight: 22; source: day.modelData.icon; isMask: true; color: card.valueColor }
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Text { text: day.modelData.highText; font: card.detailFont; color: card.valueColor }
                        Text { text: day.modelData.lowText; font: card.detailFont; color: card.detailColor }
                    }
                    Text { Layout.alignment: Qt.AlignHCenter; text: day.modelData.rainText; font: card.detailFont; color: card.detailColor }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true; implicitHeight: 0 }
        Text {
            Layout.fillWidth: true
            objectName: "staleMessage"
            visible: (card.weather?.stale ?? false) || (card.weather?.error.length ?? 0) > 0
            text: card.weather?.error || qsTr("Showing an older forecast.")
            wrapMode: Text.WordWrap
            font: card.detailFont
            color: card.detailColor
        }
        Text {
            Layout.fillWidth: true
            id: attribution
            text: "<a href='" + (card.weather?.providerUrl ?? "https://open-meteo.com/") + "'>" + (card.weather?.providerName ?? "Open-Meteo")
                + "</a> · CC BY 4.0" + ((card.weather?.updateText ?? "") ? " · " + card.weather.updateText : "")
            font: card.detailFont
            color: card.detailColor
            linkColor: card.detailColor
            wrapMode: Text.WordWrap
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
        Text {
            Layout.fillWidth: true
            text: [card.weather?.forecastText ?? "", card.weather?.automaticRefreshText ?? ""].filter(Boolean).join(" · ")
            font: card.detailFont
            color: card.detailColor
            elide: Text.ElideRight
        }
    }
}

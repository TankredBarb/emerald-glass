// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Controls.Popup {
    id: popup
    objectName: "forecastPopup"
    property var weather
    property string cityName
    property Item wallpaper
    property var material
    property font titleFont
    property font detailFont
    property color valueColor
    property color titleColor
    property color detailColor
    FontMetrics { id: fontMetrics; font: popup.detailFont }
    readonly property var columnWidths: {
        fontMetrics.font // Track font changes as well as forecast data.
        const widths = [116, 104, 88, 92]
        const headings = [qsTr("Day"), qsTr("Max / min"), qsTr("Precipitation"), qsTr("Wind max")]
        const rows = (popup.weather?.days ?? []).map(day => [day.dateText ?? day.dayText,
            day.highText + " / " + day.lowText, day.rainText, day.windText ?? "—"])
        rows.push(headings)
        for (const row of rows) {
            for (let i = 0; i < widths.length; ++i)
                widths[i] = Math.max(widths[i], Math.ceil(fontMetrics.advanceWidth(row[i])) + 8)
        }
        return widths
    }
    readonly property real minimumTableWidth: columnWidths.reduce((sum, width) => sum + width, 96)
    parent: Controls.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Math.max(680, minimumTableWidth + 40), (parent?.width ?? 712) - 32)
    height: Math.min(640, (parent?.height ?? 672) - 32)
    padding: 20
    focus: true
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside
    background: GlassCard {
        wallpaper: popup.wallpaper
        opticsEnabled: popup.material?.opticsEnabled ?? true
        refraction: popup.material?.refraction ?? 1
        dispersion: popup.material?.dispersion ?? 0.25
        specular: popup.material?.specular ?? 1
        hoverGlow: popup.material?.hoverGlow ?? 0
        blurRadius: popup.material?.blurRadius ?? 2
        materialOpacity: popup.material?.materialOpacity ?? 0.4
        tintColor: popup.material?.tintColor ?? "#296b75"
        tintStrength: popup.material?.tintStrength ?? 0.2
        adaptiveContrast: popup.material?.adaptiveContrast ?? 1
        cornerRadius: popup.material?.cornerRadius ?? 20
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: popup.cityName; font: popup.titleFont; color: popup.titleColor; elide: Text.ElideRight }
                Text { text: qsTr("Forecast · %1 days").arg(popup.weather?.days.length ?? 0); font: popup.detailFont; color: popup.detailColor }
            }
            Controls.BusyIndicator { implicitWidth: 24; implicitHeight: 24; running: popup.weather?.loading ?? false; visible: running }
            Controls.ToolButton {
                icon.name: "view-refresh"
                icon.color: popup.detailColor
                enabled: popup.weather?.canRefresh ?? false
                onClicked: popup.weather.refresh()
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Refresh")
                Accessible.name: qsTr("Refresh")
            }
            Controls.ToolButton { icon.name: "window-close"; icon.color: popup.detailColor; onClicked: popup.close(); Accessible.name: qsTr("Close forecast") }
        }
        Flickable {
            id: tableViewport
            objectName: "forecastTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: Math.max(width, popup.minimumTableWidth)
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                width: tableViewport.contentWidth
                height: Math.max(0, tableViewport.height - (tableViewport.contentWidth > tableViewport.width ? horizontalBar.height : 0))
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Layout.rightMargin: 12
                    Text { Layout.minimumWidth: popup.columnWidths[0]; Layout.maximumWidth: popup.columnWidths[0]; text: qsTr("Day"); color: popup.detailColor; font: popup.detailFont }
                    Item { Layout.fillWidth: true }
                    Text { Layout.minimumWidth: popup.columnWidths[1]; Layout.maximumWidth: popup.columnWidths[1]; text: qsTr("Max / min"); color: popup.detailColor; font: popup.detailFont; horizontalAlignment: Text.AlignRight }
                    Text { Layout.minimumWidth: popup.columnWidths[2]; Layout.maximumWidth: popup.columnWidths[2]; text: qsTr("Precipitation"); color: popup.detailColor; font: popup.detailFont; horizontalAlignment: Text.AlignRight }
                    Text { Layout.minimumWidth: popup.columnWidths[3]; Layout.maximumWidth: popup.columnWidths[3]; text: qsTr("Wind max"); color: popup.detailColor; font: popup.detailFont; horizontalAlignment: Text.AlignRight }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: popup.detailColor; opacity: 0.25 }
                ListView {
                    id: forecastList
                    objectName: "extendedDays"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: popup.weather?.days ?? []
                    spacing: 2
                    delegate: Item {
                        id: day
                        required property var modelData
                        width: Math.max(0, forecastList.width - 12)
                        implicitHeight: Math.max(44, row.implicitHeight + 16)
                        RowLayout {
                            id: row
                            anchors.fill: parent
                            spacing: 12
                            Text { Layout.minimumWidth: popup.columnWidths[0]; Layout.maximumWidth: popup.columnWidths[0]; text: day.modelData.dateText ?? day.modelData.dayText; font: popup.detailFont; color: popup.titleColor; elide: Text.ElideRight }
                            Kirigami.Icon {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                source: day.modelData.icon
                                isMask: true
                                color: popup.valueColor
                                Controls.ToolTip.visible: conditionHover.hovered
                                Controls.ToolTip.text: day.modelData.condition ?? ""
                                HoverHandler { id: conditionHover }
                            }
                            Item { Layout.fillWidth: true }
                            Text { Layout.minimumWidth: popup.columnWidths[1]; Layout.maximumWidth: popup.columnWidths[1]; text: day.modelData.highText + " / " + day.modelData.lowText; font: popup.detailFont; color: popup.valueColor; horizontalAlignment: Text.AlignRight }
                            Text { Layout.minimumWidth: popup.columnWidths[2]; Layout.maximumWidth: popup.columnWidths[2]; text: day.modelData.rainText; font: popup.detailFont; color: popup.detailColor; horizontalAlignment: Text.AlignRight }
                            Text { Layout.minimumWidth: popup.columnWidths[3]; Layout.maximumWidth: popup.columnWidths[3]; text: day.modelData.windText ?? "—"; font: popup.detailFont; color: popup.detailColor; horizontalAlignment: Text.AlignRight }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: popup.detailColor; opacity: 0.1 }
                    }
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                    Text {
                        anchors.centerIn: parent
                        width: parent.width
                        visible: forecastList.count === 0
                        text: popup.weather?.loading ? qsTr("Loading forecast…") : (popup.weather?.error || qsTr("No forecast available."))
                        color: popup.detailColor
                        font: popup.detailFont
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Controls.ScrollBar.horizontal: Controls.ScrollBar { id: horizontalBar }
        }
        Text {
            Layout.fillWidth: true
            visible: popup.weather?.hasData ?? false
            text: qsTr("Forecast confidence decreases with lead time. Daily values cover available forecast hours.")
            font: popup.detailFont
            color: popup.detailColor
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: (popup.weather?.stale ?? false) || (popup.weather?.error.length ?? 0) > 0
            text: popup.weather?.error || qsTr("Showing an older forecast.")
            wrapMode: Text.WordWrap
            font: popup.detailFont
            color: popup.detailColor
        }
        Text {
            Layout.fillWidth: true
            text: (popup.weather?.providerName ?? "") + " · CC BY 4.0 · " + (popup.weather?.updateText ?? "")
            font: popup.detailFont
            color: popup.detailColor
            wrapMode: Text.WordWrap
        }
        Text { text: popup.weather?.automaticRefreshText ?? ""; font: popup.detailFont; color: popup.detailColor }
    }
}

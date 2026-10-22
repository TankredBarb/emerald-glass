// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

Item {
    id: monitor
    property var sensorIds: ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]
    property int sensorColumns: 3
    property var sensorLabels: ({})
    readonly property int effectiveColumns: Math.max(1, Math.min(sensorIds.length, sensorColumns > 0 ? sensorColumns : sensorIds.length))
    property Item wallpaper
    property alias material: glass
    property font valueFont: Qt.font({family: "sans-serif", pixelSize: 27, weight: Font.DemiBold})
    property font titleFont: Qt.font({family: "sans-serif", pixelSize: 12, weight: Font.DemiBold})
    property font detailFont: Qt.font({family: "sans-serif", pixelSize: 10})
    property bool unifiedTextColor: true
    property color valueColor: "#f5fcff"
    property color titleColor: "#e5f3f8"
    property color detailColor: "#b5c9d2"
    property color cpuGraphColor: "#63e5ef"
    property color gpuTemperatureGraphColor: "#ffd27a"
    property color cpuTemperatureGraphColor: "#f6a0cf"
    property bool chartOutline: true
    readonly property real minimumCardHeight: graphs.implicitHeight + 40 + (graphs.width > viewport.width ? scrollBar.height : 0)
    implicitWidth: 496
    implicitHeight: Math.max(164, minimumCardHeight)

    GlassCard {
        id: glass
        anchors.fill: parent
        wallpaper: monitor.wallpaper
    }

    Flickable {
        id: viewport
        objectName: "sensorViewport"
        anchors.fill: parent
        anchors.margins: 20
        clip: true
        contentWidth: graphs.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        GridLayout {
            id: graphs
            columns: monitor.effectiveColumns
            uniformCellWidths: true
            uniformCellHeights: true
            width: Math.max(viewport.width, monitor.effectiveColumns * 100 + (monitor.effectiveColumns - 1) * 16)
            height: Math.max(0, viewport.height - (width > viewport.width ? scrollBar.height : 0))
            columnSpacing: 16
            rowSpacing: 16
            Repeater {
                objectName: "selectedSensorCards"
                model: monitor.sensorIds
                delegate: Item {
                    id: cell
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 100
                    implicitHeight: sensorGraph.implicitHeight
                    SensorGraph {
                        id: sensorGraph
                        anchors.fill: parent
                        sensorId: cell.modelData
                        customTitle: monitor.sensorLabels[sensorId] || ""
                        title: sensorId === "cpu/all/usage" || sensorId === "cpu/all/averageTemperature" ? "CPU"
                            : sensorId === "gpu/gpu0/temperature" ? "GPU" : ""
                        detail: sensorId === "cpu/all/usage" ? qsTr("Load")
                            : sensorId === "cpu/all/averageTemperature" || sensorId === "gpu/gpu0/temperature" ? qsTr("Temperature") : ""
                        legacyRange: title.length > 0
                        accent: sensorId === "cpu/all/usage" ? monitor.cpuGraphColor
                            : sensorId === "gpu/gpu0/temperature" ? monitor.gpuTemperatureGraphColor
                            : sensorId === "cpu/all/averageTemperature" ? monitor.cpuTemperatureGraphColor
                            : ["#63e5ef", "#ffd27a", "#f6a0cf", "#94df9c", "#b6a4ff", "#ffad80"][cell.index % 6]
                        chartOutline: monitor.chartOutline
                        textStyle.valueFont: monitor.valueFont
                        textStyle.titleFont: monitor.titleFont
                        textStyle.detailFont: monitor.detailFont
                        textStyle.valueColor: monitor.valueColor
                        textStyle.titleColor: monitor.unifiedTextColor ? monitor.valueColor : monitor.titleColor
                        textStyle.detailColor: monitor.unifiedTextColor ? monitor.valueColor : monitor.detailColor
                    }
                    Rectangle { objectName: "verticalDivider"; x: -8; width: 1; height: parent.height; color: monitor.unifiedTextColor ? monitor.valueColor : monitor.detailColor; opacity: 0.2; visible: cell.index % monitor.effectiveColumns > 0 }
                    Rectangle { objectName: "horizontalDivider"; y: -8; height: 1; width: parent.width; color: monitor.unifiedTextColor ? monitor.valueColor : monitor.detailColor; opacity: 0.2; visible: cell.index >= monitor.effectiveColumns }
                }
            }
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar { id: scrollBar }
    }
    Text {
        renderType: Text.QtRendering
        anchors.centerIn: parent
        width: parent.width - 40
        visible: monitor.sensorIds.length === 0
        text: qsTr("Choose sensors in the widget settings.")
        font: monitor.detailFont
        color: monitor.unifiedTextColor ? monitor.valueColor : monitor.detailColor
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}

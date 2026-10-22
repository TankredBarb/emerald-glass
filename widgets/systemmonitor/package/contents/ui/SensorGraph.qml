// SPDX-License-Identifier: LGPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.ksysguard.sensors as Sensors
import org.kde.quickcharts as Charts

ColumnLayout {
    id: graph
    objectName: sensorId
    property string title
    property string customTitle
    property string detail
    property string sensorId
    property color accent
    property alias textStyle: textBlock
    property bool chartOutline: true
    property var measuredValue
    property string formattedValue
    property string sensorName
    property string sensorDescription
    property real rangeMinimum: 0
    property real rangeMaximum: 100
    property bool legacyRange: false
    readonly property bool numeric: typeof measuredValue === "number" && Number.isFinite(measuredValue)
    readonly property bool automaticRange: !legacyRange && !(Number.isFinite(rangeMaximum) && rangeMaximum > rangeMinimum)
    onSensorIdChanged: {
        measuredValue = undefined
        formattedValue = ""
        sensorName = ""
        sensorDescription = ""
    }
    readonly property bool ready: measuredValue !== undefined && measuredValue !== null
    spacing: 3

    // Sensor.value supplies a typed zero before the first daemon reading.
    // SensorDataModel keeps that reading invalid, so startup is not graphed as 0.
    Sensors.SensorDataModel {
        id: sensorData
        sensors: graph.sensorId ? [graph.sensorId] : []
        updateRateLimit: 1000
        enabled: graph.visible
        function refreshValue() {
            const hasColumn = columnCount() > 0 && rowCount() > 0
            graph.sensorName = hasColumn ? (data(index(0, 0), Sensors.SensorDataModel.Name) ?? "") : ""
            graph.sensorDescription = hasColumn ? (data(index(0, 0), Sensors.SensorDataModel.Description) ?? "") : ""
            graph.rangeMinimum = hasColumn ? sensorData.minimum : 0
            graph.rangeMaximum = hasColumn ? sensorData.maximum : 0
            graph.measuredValue = hasColumn ? data(index(0, 0), Sensors.SensorDataModel.Value) : undefined
            graph.formattedValue = hasColumn ? (data(index(0, 0), Sensors.SensorDataModel.FormattedValue) ?? "") : ""
        }
        onDataChanged: refreshValue()
        onReadyChanged: refreshValue()
        onSensorMetaDataChanged: refreshValue()
        onColumnsInserted: refreshValue()
        onColumnsRemoved: refreshValue()
        onModelReset: { graph.measuredValue = undefined; graph.formattedValue = "" }
    }

    SensorText {
        id: textBlock
        Layout.fillWidth: true
        title: graph.customTitle || graph.title || graph.sensorName || graph.sensorId
        valueText: graph.ready ? graph.formattedValue : "—"
        detail: graph.ready ? (graph.detail || graph.sensorDescription) : qsTr("Sensor unavailable")
        accent: graph.accent
    }

    Loader {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 28
        active: graph.ready && graph.numeric
        sourceComponent: Charts.LineChart {
            id: chart
            objectName: "historyChart"
            direction: Charts.XYChart.ZeroAtEnd
            lineWidth: 1.6
            fillOpacity: 0.12
            interpolate: true
            yRange {
                from: graph.legacyRange ? 0 : graph.rangeMinimum
                to: graph.legacyRange ? 100 : graph.rangeMaximum
                automatic: graph.automaticRange
            }
            colorSource: Charts.SingleValueSource { value: graph.accent }
            valueSources: [Charts.HistoryProxySource {
                source: Charts.SingleValueSource { value: graph.measuredValue }
                maximumHistory: 60
                interval: graph.visible && graph.ready ? 1000 : 0
                fillMode: Charts.HistoryProxySource.DoNotFill
            }]

            Charts.LineChart {
                anchors.fill: parent
                z: -1
                visible: graph.chartOutline
                direction: chart.direction
                lineWidth: chart.lineWidth + 2
                fillOpacity: 0
                interpolate: chart.interpolate
                yRange { from: chart.yRange.from; to: chart.yRange.to; automatic: chart.yRange.automatic }
                colorSource: Charts.SingleValueSource {
                    value: graph.accent.r * 0.2126 + graph.accent.g * 0.7152 + graph.accent.b * 0.0722 > 0.5
                        ? "#c0000000" : "#d9ffffff"
                }
                valueSources: chart.valueSources
            }
        }
    }
}

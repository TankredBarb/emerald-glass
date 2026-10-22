// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtTest
import org.kde.kirigami as Kirigami
import "../package/contents/ui" as Monitor
import "../package/contents/ui/SensorLabels.js" as SensorLabels

Item {
    width: 620
    height: 660
    Kirigami.PageRow {
        anchors.fill: parent
        initialPage: Monitor.ConfigSensors { id: settings }
    }
    Monitor.SystemMonitor {
        id: monitor
        width: 400
        height: 180
        visible: false
        sensorIds: settings.cfg_sensorIds
        sensorColumns: settings.cfg_sensorColumns
        sensorLabels: SensorLabels.parse(settings.cfg_sensorLabels)
    }
    Monitor.SensorGraph { id: sample; width: 300; height: 160; accent: "cyan" }
    TestCase {
        name: "SensorSelection"
        when: windowShown
        function init() {
            settings.cfg_sensorIds = ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]
            settings.cfg_sensorColumns = 3
            settings.cfg_sensorLabels = "{}"
            sample.measuredValue = undefined
            sample.formattedValue = ""
            sample.legacyRange = false
            sample.rangeMinimum = 0
            sample.rangeMaximum = 100
        }
        function test_schema_and_saved_selection() {
            compare(schemaValues.sensorIds, ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"])
            const ids = ["memory/physical/used", "network/all/download", "cpu/all/usage"]
            compare(schemaValues.sensorColumns, 3)
            compare(schemaValues.sensorLabels, "{}")
            const grid = sensorConfig.roundTripSettings(ids, 2, "{}")
            compare(grid.ids, ids)
            compare(grid.columns, 2)
            const empty = sensorConfig.roundTripSettings([], 0, "{}")
            compare(empty.ids, [])
            compare(empty.columns, 0)
        }
        function test_names_follow_ids_and_preserve_sensor_ranges() {
            const selection = findChild(settings, "sensorSelection")
            const cpuId = "cpu/all/usage"
            const missingId = "missing-device/no-such-sensor"
            const cpuName = 'ЦП "нагрузка", %'
            selection.sensorLabelChanged(cpuId, cpuName)
            compare(findChild(monitor, cpuId).textStyle.title, cpuName)
            selection.selected = [missingId, cpuId]
            selection.sensorLabelChanged(missingId, "Мой датчик")
            const missing = findChild(monitor, missingId)
            compare(missing.textStyle.title, "Мой датчик")
            verify(!missing.legacyRange, "A custom title must not enable the legacy 0–100 range")
            compare(missing.textStyle.valueText, "—")
            const saved = sensorConfig.roundTripSettings(selection.selected, 2, settings.cfg_sensorLabels)
            settings.cfg_sensorLabels = "{}"
            settings.cfg_sensorLabels = saved.labels
            selection.selected = [cpuId, missingId]
            compare(findChild(monitor, cpuId).textStyle.title, cpuName)
            compare(findChild(monitor, missingId).textStyle.title, "Мой датчик")
            selection.selected = [missingId]
            selection.selected = [missingId, cpuId]
            compare(findChild(monitor, cpuId).textStyle.title, cpuName)
            selection.sensorLabelChanged(cpuId, "   ")
            compare(findChild(monitor, cpuId).textStyle.title, "CPU")
            selection.sensorLabelChanged(missingId, "")
            compare(findChild(monitor, missingId).textStyle.title, missingId)
        }
        function test_invalid_saved_names_do_not_break_display_or_editing() {
            const selection = findChild(settings, "sensorSelection")
            for (const serialized of ["", "{broken", "null", "[]", '"CPU"', "7", '{"cpu/all/usage":42}']) {
                settings.cfg_sensorLabels = serialized
                compare(findChild(monitor, "cpu/all/usage").textStyle.title, "CPU")
                selection.sensorLabelChanged("cpu/all/usage", "Мой ЦП")
                compare(findChild(monitor, "cpu/all/usage").textStyle.title, "Мой ЦП")
                compare(JSON.parse(settings.cfg_sensorLabels)["cpu/all/usage"], "Мой ЦП")
            }
        }
        function findEditorControl(item, editing) {
            if (editing && item instanceof Controls.TextField && item.activeFocus) return item
            if (!editing && "icon" in item && item.icon.name === "document-edit") return item
            for (const child of item.children) {
                const control = findEditorControl(child, editing)
                if (control) return control
            }
            return null
        }
        function test_native_name_editor_accepts_and_updates_widget() {
            settings.cfg_sensorIds = ["cpu/all/usage"]
            const selection = findChild(settings, "sensorSelection")
            verify(selection.labelsEditable)
            tryVerify(() => findEditorControl(selection, false) !== null)
            mouseClick(findEditorControl(selection, false))
            tryVerify(() => findEditorControl(selection, true) !== null)
            const editor = findEditorControl(selection, true)
            editor.text = "Мой процессор"
            keyClick(Qt.Key_Return)
            compare(JSON.parse(settings.cfg_sensorLabels)["cpu/all/usage"], "Мой процессор")
            compare(findChild(monitor, "cpu/all/usage").textStyle.title, "Мой процессор")
        }
        function test_add_remove_reorder_and_empty_selection() {
            const selection = findChild(settings, "sensorSelection")
            selection.selected = ["cpu/all/usage", "memory/physical/used", "network/all/download"]
            compare(settings.cfg_sensorIds, selection.selected)
            let cards = findChild(monitor, "selectedSensorCards")
            compare(cards.count, 3)
            compare(cards.itemAt(1).modelData, "memory/physical/used")
            // Native Choices publishes edits through selectedChanged, including in-place drag/drop.
            selection.selected = ["network/all/download", "cpu/all/usage"]
            compare(monitor.sensorIds, ["network/all/download", "cpu/all/usage"])
            compare(cards.count, 2)
            compare(cards.itemAt(0).modelData, "network/all/download")
            selection.selected = []
            compare(cards.count, 0)
            compare(monitor.sensorIds.length, 0)
        }
        function test_unavailable_sensor_is_retained() {
            settings.cfg_sensorIds = ["missing-device/no-such-sensor"]
            let graph = findChild(monitor, "missing-device/no-such-sensor")
            verify(graph !== null)
            verify(!graph.ready)
            compare(graph.textStyle.valueText, "—")
            compare(settings.cfg_sensorIds, ["missing-device/no-such-sensor"])
        }
        function test_units_and_ranges() {
            sample.measuredValue = 4096
            sample.formattedValue = "4.0 KiB/s"
            sample.sensorName = "Download rate"
            sample.rangeMaximum = 1048576
            tryVerify(() => findChild(sample, "historyChart") !== null)
            const chart = findChild(sample, "historyChart")
            compare(chart.yRange.automatic, false)
            compare(chart.yRange.to, 1048576)
            compare(sample.textStyle.title, "Download rate")
            compare(sample.textStyle.valueText, "4.0 KiB/s")
            sample.rangeMaximum = 0
            compare(chart.yRange.automatic, true)
            sample.rangeMinimum = -30
            sample.rangeMaximum = 120
            compare(chart.yRange.from, -30)
            compare(chart.yRange.to, 120)
            sample.measuredValue = "enabled"
            sample.formattedValue = "Enabled"
            compare(sample.textStyle.valueText, "Enabled")
            tryCompare(sample, "numeric", false)
            tryVerify(() => findChild(sample, "historyChart") === null)
        }
        function test_grid_rows_and_partial_row() {
            settings.cfg_sensorIds = ["a/1", "a/2", "a/3", "a/4", "a/5"]
            settings.cfg_sensorColumns = 2
            monitor.visible = true
            try {
                const cards = findChild(monitor, "selectedSensorCards")
                tryVerify(() => cards.itemAt(2).y > cards.itemAt(0).y)
                compare(cards.itemAt(0).y, cards.itemAt(1).y)
                compare(cards.itemAt(0).x, cards.itemAt(2).x)
                verify(cards.itemAt(4).y > cards.itemAt(2).y)
                compare(cards.itemAt(4).width, cards.itemAt(0).width)
                const gridHeight = monitor.minimumCardHeight
                settings.cfg_sensorColumns = 0
                tryVerify(() => monitor.minimumCardHeight < gridHeight)
                compare(cards.itemAt(4).y, cards.itemAt(0).y)
                settings.cfg_sensorColumns = 1
                tryVerify(() => cards.itemAt(1).y > cards.itemAt(0).y)
                compare(cards.itemAt(1).x, cards.itemAt(0).x)
                verify(monitor.minimumCardHeight > gridHeight)
                settings.cfg_sensorIds = []
                compare(monitor.effectiveColumns, 1)
                verify(Number.isFinite(monitor.minimumCardHeight))
            } finally {
                monitor.visible = false
            }
        }
        function test_many_sensors_are_scrollable() {
            settings.cfg_sensorColumns = 0
            settings.cfg_sensorIds = ["a/1", "a/2", "a/3", "a/4", "a/5", "a/6", "a/7", "a/8"]
            const viewport = findChild(monitor, "sensorViewport")
            verify(viewport.contentWidth > viewport.width)
            compare(findChild(monitor, "selectedSensorCards").count, 8)
        }
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Dialogs
import QtTest
import org.kde.kirigami as Kirigami
import "../package/contents/ui" as Monitor

Item {
    id: scene
    width: 620
    height: 660
    Kirigami.PageRow {
        anchors.fill: parent
        initialPage: Monitor.ConfigText { id: settings }
    }
    Monitor.SystemMonitor {
        id: monitor
        visible: false
        valueFont: settings.cfg_valueFont
        titleFont: settings.cfg_titleFont
        detailFont: settings.cfg_detailFont
        unifiedTextColor: settings.cfg_unifiedTextColor
        valueColor: settings.cfg_valueColor
        titleColor: settings.cfg_titleColor
        detailColor: settings.cfg_detailColor
        cpuGraphColor: settings.cfg_cpuGraphColor
        gpuTemperatureGraphColor: settings.cfg_gpuTemperatureGraphColor
        cpuTemperatureGraphColor: settings.cfg_cpuTemperatureGraphColor
        chartOutline: settings.cfg_chartOutline
    }
    FontMetrics { id: titleMetrics; font: settings.cfg_titleFont }
    TestCase {
        name: "TextAndChartSettings"
        when: windowShown
        function init() {
            settings.restoreDefaults()
            settings.restoreChartDefaults()
        }
        function test_kconfig_schema_loads_valid_fonts_and_colors() {
            compare(schemaValues.valueFont.family, "sans-serif")
            compare(schemaValues.titleFont.family, "sans-serif")
            compare(schemaValues.detailFont.family, "sans-serif")
            compare(schemaValues.valueColor, Qt.color("#f5fcff"))
            compare(schemaValues.titleColor, Qt.color("#e5f3f8"))
            compare(schemaValues.detailColor, Qt.color("#b5c9d2"))
            compare(schemaValues.cpuGraphColor, Qt.color("#63e5ef"))
            compare(schemaValues.gpuTemperatureGraphColor, Qt.color("#ffd27a"))
            compare(schemaValues.cpuTemperatureGraphColor, Qt.color("#f6a0cf"))
            compare(schemaValues.tintColor, Qt.color("#296b75"))
        }
        function test_text_controls_update_preview_and_all_sensors() {
            settings.cfg_unifiedTextColor = false
            settings.cfg_valueFont = Qt.font({family: "monospace", pixelSize: 35, weight: Font.Bold, italic: true})
            settings.cfg_titleFont = Qt.font({family: "serif", pixelSize: 15})
            settings.cfg_detailFont = Qt.font({family: "sans-serif", pixelSize: 13})
            settings.cfg_valueColor = "#ffcc88"
            settings.cfg_titleColor = "#102030"
            settings.cfg_detailColor = "#445566"
            let preview = findChild(settings, "textPreview")
            let ids = ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]
            let blocks = [preview]
            for (let id of ids) blocks.push(findChild(monitor, id).textStyle)
            for (let block of blocks) {
                compare(block.valueFont.family, "monospace")
                compare(block.valueFont.pixelSize, 35)
                compare(block.valueFont.weight, Font.Bold)
                compare(block.valueFont.italic, true)
                compare(block.titleFont.pixelSize, 15)
                compare(block.detailFont.pixelSize, 13)
                compare(block.valueColor, Qt.color("#ffcc88"))
                compare(block.titleColor, Qt.color("#102030"))
                compare(block.detailColor, Qt.color("#445566"))
            }
        }
        function test_shared_color_reaches_captions_and_dividers() {
            verify(schemaValues.unifiedTextColor)
            settings.cfg_unifiedTextColor = true
            settings.cfg_valueColor = "#ffcc88"
            settings.cfg_titleColor = "#102030"
            settings.cfg_detailColor = "#445566"
            const preview = findChild(settings, "textPreview")
            compare(preview.titleColor, Qt.color("#ffcc88"))
            compare(preview.detailColor, Qt.color("#ffcc88"))
            for (const id of ["cpu/all/usage", "gpu/gpu0/temperature", "cpu/all/averageTemperature"]) {
                const graph = findChild(monitor, id)
                compare(graph.textStyle.valueColor, Qt.color("#ffcc88"))
                compare(graph.textStyle.titleColor, Qt.color("#ffcc88"))
                compare(graph.textStyle.detailColor, Qt.color("#ffcc88"))
            }
            compare(findChild(monitor, "verticalDivider").color, Qt.color("#ffcc88"))
            compare(findChild(monitor, "horizontalDivider").color, Qt.color("#ffcc88"))
            settings.cfg_valueColor = "#80c0ff"
            compare(findChild(monitor, "verticalDivider").color, Qt.color("#80c0ff"))
            settings.cfg_unifiedTextColor = false
            compare(preview.titleColor, Qt.color("#102030"))
            compare(preview.detailColor, Qt.color("#445566"))
            compare(findChild(monitor, "verticalDivider").color, Qt.color("#445566"))
        }
        function test_names_are_displayed_as_literal_text() {
            const preview = findChild(settings, "textPreview")
            const original = preview.title
            try {
                preview.title = "<b>CPU & GPU</b>"
                const label = preview.children[0].children.find(item => item.text === preview.title)
                verify(label !== undefined)
                tryVerify(() => Math.abs(label.contentWidth - titleMetrics.advanceWidth(preview.title)) < 2,
                    1000, "A sensor name must show markup characters literally")
            } finally {
                preview.title = original
            }
        }
        function test_large_font_increases_card_minimum_height() {
            let before = monitor.minimumCardHeight
            settings.cfg_valueFont = Qt.font({family: "sans-serif", pixelSize: 60})
            tryVerify(() => monitor.minimumCardHeight > before + 20)
        }
        function test_chart_colors_and_outline_reach_each_sensor() {
            settings.cfg_cpuGraphColor = "red"
            settings.cfg_gpuTemperatureGraphColor = "green"
            settings.cfg_cpuTemperatureGraphColor = "blue"
            settings.cfg_chartOutline = false
            let cpu = findChild(monitor, "cpu/all/usage")
            let gpu = findChild(monitor, "gpu/gpu0/temperature")
            let temperature = findChild(monitor, "cpu/all/averageTemperature")
            compare(cpu.accent, Qt.color("red"))
            compare(gpu.accent, Qt.color("green"))
            compare(temperature.accent, Qt.color("blue"))
            compare(cpu.chartOutline, false)
            compare(gpu.chartOutline, false)
            compare(temperature.chartOutline, false)
        }
        function test_font_dialog_cancel_preserves_saved_selection() {
            let button = findChild(settings, "valueFontButton")
            let dialog = findChild(button, "fontDialog")
            dialog.options = FontDialog.DontUseNativeDialog
            button.clicked()
            tryCompare(dialog, "visible", true)
            dialog.selectedFont = Qt.font({family: "serif", pixelSize: 40})
            dialog.reject()
            compare(settings.cfg_valueFont.pixelSize, 27)
            compare(settings.cfg_valueFont.family, "sans-serif")
        }
        function test_font_dialog_accept_updates_selection() {
            let button = findChild(settings, "valueFontButton")
            let dialog = findChild(button, "fontDialog")
            dialog.options = FontDialog.DontUseNativeDialog
            button.clicked()
            tryCompare(dialog, "visible", true)
            wait(100)
            dialog.selectedFont = Qt.font({family: "DejaVu Serif", pointSize: 30, italic: true})
            wait(100)
            dialog.accept()
            compare(settings.cfg_valueFont.pointSize, 30)
            compare(settings.cfg_valueFont.italic, true)
        }
    }
}

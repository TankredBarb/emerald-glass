// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Window
import QtTest
import "../package/contents/ui" as Monitor

Item {
    id: scene
    width: 400
    height: 260
    Rectangle { id: wallpaper; anchors.fill: parent; color: "white" }
    Monitor.GlassCard {
        x: 20
        y: 20
        width: 360
        height: 220
        wallpaper: wallpaper
        materialOpacity: 0
        blurRadius: 0
        adaptiveContrast: 0
        tintStrength: 0
        specular: 0
    }
    Monitor.SensorGraph {
        id: graph
        x: 40
        y: 40
        width: 320
        height: 180
        title: "CPU"
        detail: "Temperature"
        formattedValue: "50 °C"
        accent: "white"
    }
    TestCase {
        name: "ChartsOnTransparentGlass"
        when: windowShown
        function test_outline_preserves_line_on_bright_and_dark_wallpaper() {
            graph.measuredValue = 50
            wait(2200)
            let chart = findChild(graph, "historyChart")
            verify(chart !== null)
            const p = chart.mapToItem(scene, chart.width / 2, chart.height / 2)
            const scale = Screen.devicePixelRatio

            graph.chartOutline = false
            wait(100)
            let plain = grabImage(scene)
            graph.chartOutline = true
            wait(100)
            let outlined = grabImage(scene)
            let darkening = 0
            for (let y = -4; y <= 4; ++y) {
                const px = Math.round(p.x * scale)
                const py = Math.round((p.y + y) * scale)
                darkening += plain.red(px, py) - outlined.red(px, py)
            }
            verify(darkening > 100, "The outline must separate a white chart from white wallpaper")

            wallpaper.color = "black"
            wait(100)
            let dark = grabImage(scene)
            let brightest = 0
            for (let y = -2; y <= 2; ++y) {
                brightest = Math.max(brightest, dark.red(Math.round(p.x * scale), Math.round((p.y + y) * scale)))
            }
            verify(brightest > 180, "The colored line must remain visible on dark wallpaper")

            graph.accent = "black"
            wait(100)
            let darkLine = grabImage(scene)
            brightest = 0
            for (let y = -4; y <= 4; ++y) {
                brightest = Math.max(brightest, darkLine.red(Math.round(p.x * scale), Math.round((p.y + y) * scale)))
            }
            verify(brightest > 150, "A dark line needs a light outline on dark wallpaper")
        }
    }
}

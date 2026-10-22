// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtTest
import org.kde.kirigami as Kirigami
import "../package/contents/ui" as Weather

Item {
    width: 620
    height: 660
    Kirigami.PageRow {
        anchors.fill: parent
        id: pages
        initialPage: Weather.ConfigLocation { id: location }
    }
    QtObject {
        id: forecast
        property bool hasData: true
        property bool loading: false
        property bool stale: false
        property bool canRefresh: true
        property string error: ""
        property string automaticRefreshText: "Auto refresh 12:30"
        property string nextRefreshText: "12:00"
        property string providerName: "Open-Meteo"
        property string providerUrl: "https://open-meteo.com/"
        property string updateText: "Updated 21.10 12:00"
        property string forecastText: "Forecast for 21.10 12:00"
        property var current: ({temperatureText: "0 °C", condition: "Clear", icon: "weather-clear", windText: "—", humidityText: "50%", pressureText: "1000 hPa"})
        property var hours: []
        property var days: []
        function refresh() {}
    }
    Weather.WeatherCard {
        id: card
        width: 496
        height: minimumCardHeight
        weather: forecast
        material.materialOpacity: 0
        material.blurRadius: 0
    }
    Component { id: textPage; Weather.ConfigText {} }
    Component { id: effectsPage; Weather.ConfigEffects {} }
    TestCase {
        name: "WeatherLocation"
        when: windowShown
        function init() {
            forecast.hasData = true
            forecast.loading = false
            forecast.stale = false
            forecast.error = ""
        }
        function test_card_states_and_transparent_material() {
            const rows = []
            for (let i = 0; i < 6; ++i) rows.push({timeText: "12:00", icon: "weather-clear", temperatureText: "0°"})
            forecast.hours = rows
            forecast.days = [{dayText: "Wed", icon: "weather-clear", highText: "1°", lowText: "0°", rainText: "—"}]
            wait(50)
            verify(card.minimumCardHeight >= 320)
            compare(card.material.materialOpacity, 0)
            compare(card.material.blurRadius, 0)
            compare(findChild(card, "temperature").text, "0 °C")
            const strip = findChild(card, "hours")
            verify(strip.children[5].x > 300, "Hours should fill the width of the card")
            forecast.hasData = false
            forecast.loading = true
            wait(1)
            verify(findChild(card, "loadingMessage").visible)
            forecast.loading = false
            forecast.error = "No network"
            wait(1)
            compare(findChild(card, "loadingMessage").text, "No network")
            forecast.hasData = true
            forecast.stale = true
            wait(1)
            verify(findChild(card, "staleMessage").visible)
            compare(findChild(card, "temperature").text, "0 °C")
        }
        function test_extended_forecast_opens_and_keeps_card_compact() {
            const rows = []
            for (let i = 0; i < 16; ++i) rows.push({dayText: "Wed", dateText: "Wed " + (i + 1) + ".11", icon: "weather-clear", condition: "Clear", highText: "10°", lowText: "0°", rainText: "1 mm", windText: "3 m/s"})
            forecast.days = rows
            const button = findChild(card, "openForecast")
            mouseClick(button)
            const popup = findChild(card, "forecastPopup")
            tryCompare(popup, "opened", true)
            compare(findChild(card, "compactDaysModel").count, 4)
            const extended = findChild(popup, "extendedDays")
            compare(extended.count, 16)
            extended.positionViewAtIndex(15, ListView.End)
            wait(50)
            verify(extended.contentY > 0)
            // The popup uses the same per-widget transparent glass settings.
            compare(popup.background.materialOpacity, 0)
            compare(popup.background.blurRadius, 0)
            keyClick(Qt.Key_Escape)
            tryCompare(popup, "opened", false)
            tryCompare(popup, "visible", false)
            mouseClick(card, 80, 80)
            tryCompare(popup, "opened", true)
            popup.close()
            tryCompare(popup, "opened", false)
        }
        function descendants(item) {
            let result = []
            for (const child of item.children ?? []) {
                result.push(child)
                result = result.concat(descendants(child))
            }
            return result
        }
        function test_large_forecast_font_fits_columns() {
            const originalFont = card.detailFont
            const popup = findChild(card, "forecastPopup")
            try {
                card.detailFont = Qt.font({family: "sans-serif", pixelSize: 24})
                card.showForecast()
                tryCompare(popup, "opened", true)
                wait(50)
                const header = descendants(popup.contentItem).find(i => i.text === "Precipitation")
                verify(header !== undefined)
                verify(header.paintedWidth <= header.width, "Configured font must fit the precipitation column")
                const table = findChild(popup, "forecastTable")
                verify(table !== null)
                verify(table.contentWidth > table.width, "A narrow window must allow horizontal scrolling at large font sizes")
                table.contentX = table.contentWidth - table.width
                wait(1)
                compare(table.contentX, table.contentWidth - table.width)
            } finally {
                popup.close()
                tryCompare(popup, "visible", false)
                card.detailFont = originalFont
            }
        }
        function test_coordinate_validator_and_parser_agree() {
            const inputs = descendants(location).filter(i => i.validator?.bottom === -90 || i.validator?.bottom === -180)
            compare(inputs.length, 2)
            for (const input of inputs) {
                const key = input.validator.bottom === -90 ? "cfg_latitude" : "cfg_longitude"
                for (const sample of [{text: "0,010", value: 10}, {text: "46.48572", value: 46.48572}, {text: "-30.74383", value: -30.74383}, {text: "0.010", value: 0.01}]) {
                    input.text = sample.text
                    verify(input.acceptableInput)
                    input.editingFinished()
                    compare(location[key], sample.value)
                }
                input.text = "999"
                verify(!input.acceptableInput)
                input.editingFinished()
                compare(location[key], 0.01)
                compare(input.text, "0.01")
                location[key] = key === "cfg_latitude" ? 46.48572 : 30.74383
                compare(input.text, location[key].toString(), "Coordinate input must follow a newly selected location after invalid input")
            }
        }
        function test_settings_pages_load() {
            const text = pages.push(textPage)
            verify(text !== null)
            compare(text.cfg_valueFont.pixelSize, 44)
            pages.pop()
            const effects = pages.push(effectsPage)
            verify(effects !== null)
            effects.cfg_materialOpacity = 0
            effects.cfg_blurRadius = 0
            compare(effects.cfg_materialOpacity, 0)
            pages.pop()
        }
        function test_coordinate_location_is_not_station_bound() {
            location.cfg_locationName = "Village"
            location.cfg_latitude = 45.35
            location.cfg_longitude = 28.83
            location.cfg_provider = 1
            compare(location.cfg_locationName, "Village")
            compare(location.cfg_latitude, 45.35)
            compare(location.cfg_longitude, 28.83)
            compare(location.cfg_provider, 1)
        }
    }
}

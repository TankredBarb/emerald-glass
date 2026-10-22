// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import "backend" as Backend

Rectangle {
    width: 900
    height: 760
    color: "#34566b"
    Image { id: backgroundImage; anchors.fill: parent; source: previewWallpaper; fillMode: Image.PreserveAspectCrop }
    Backend.WeatherBackend {
        id: backend
        Component.onCompleted: configure(previewProvider, 46.48572, 30.74383, "Europe/Kyiv", 30)
    }
    WeatherCard {
        id: forecastCard
        Component.onCompleted: if (previewExpanded) Qt.callLater(forecastCard.showForecast)
        anchors.centerIn: parent
        width: 496
        height: Math.max(360, minimumCardHeight)
        wallpaper: backgroundImage
        weather: backend
    }
}

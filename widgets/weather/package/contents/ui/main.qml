// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "backend" as Backend

PlasmoidItem {
    id: root
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    DesktopRotationFix { }
    readonly property var locationSettings: [Plasmoid.configuration.provider, Plasmoid.configuration.latitude,
        Plasmoid.configuration.longitude, Plasmoid.configuration.timeZone, Plasmoid.configuration.refreshMinutes]
    function applyLocation() {
        weatherController.configure(Plasmoid.configuration.provider, Plasmoid.configuration.latitude,
            Plasmoid.configuration.longitude, Plasmoid.configuration.timeZone, Plasmoid.configuration.refreshMinutes)
    }
    onLocationSettingsChanged: Qt.callLater(applyLocation)
    Component.onCompleted: {
        if (!Plasmoid.configuration.textDefaultsInitialized) {
            Plasmoid.configuration.valueFont = Qt.font({family: "sans-serif", pixelSize: 44, weight: Font.DemiBold})
            Plasmoid.configuration.titleFont = Qt.font({family: "sans-serif", pixelSize: 16, weight: Font.DemiBold})
            Plasmoid.configuration.detailFont = Qt.font({family: "sans-serif", pixelSize: 12})
            Plasmoid.configuration.textDefaultsInitialized = true
            // KConfigPropertyMap's invokable is missing from Plasma's QML type info.
            // qmllint disable missing-property
            Plasmoid.configuration.writeConfig()
            // qmllint enable missing-property
        }
        applyLocation()
    }
    Backend.WeatherBackend { id: weatherController }
    fullRepresentation: WeatherCard {
        Layout.minimumWidth: 420
        Layout.minimumHeight: minimumCardHeight
        Layout.preferredWidth: 496
        Layout.preferredHeight: 360
        weather: weatherController
        cityName: Plasmoid.configuration.locationName
        wallpaper: Plasmoid.containment.wallpaperGraphicsObject as Item
        material.opticsEnabled: Plasmoid.configuration.opticsEnabled
        material.refraction: Plasmoid.configuration.refraction / 100
        material.dispersion: Plasmoid.configuration.dispersion / 100
        material.specular: Plasmoid.configuration.specular / 100
        material.hoverGlow: Plasmoid.configuration.hoverGlow / 100
        material.blurRadius: Plasmoid.configuration.blurRadius / 10
        material.materialOpacity: Plasmoid.configuration.materialOpacity / 100
        material.tintColor: Plasmoid.configuration.tintColor
        material.tintStrength: Plasmoid.configuration.tintStrength / 100
        material.adaptiveContrast: Plasmoid.configuration.adaptiveContrast / 100
        material.cornerRadius: Plasmoid.configuration.cornerRadius
        valueFont: Plasmoid.configuration.valueFont
        titleFont: Plasmoid.configuration.titleFont
        detailFont: Plasmoid.configuration.detailFont
        valueColor: Plasmoid.configuration.valueColor
        titleColor: Plasmoid.configuration.titleColor
        detailColor: Plasmoid.configuration.detailColor
    }
}

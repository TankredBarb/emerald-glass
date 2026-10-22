// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "SensorLabels.js" as SensorLabels

PlasmoidItem {
    id: root
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    DesktopRotationFix { }
    // KConfigLoader's Font default is a family name, not a serialized QFont.
    // Seed sizes and weights once, then retain the user's complete fonts.
    Component.onCompleted: {
        if (!Plasmoid.configuration.textDefaultsInitialized) {
            Plasmoid.configuration.valueFont = Qt.font({family: "sans-serif", pixelSize: 27, weight: Font.DemiBold})
            Plasmoid.configuration.titleFont = Qt.font({family: "sans-serif", pixelSize: 12, weight: Font.DemiBold})
            Plasmoid.configuration.detailFont = Qt.font({family: "sans-serif", pixelSize: 10})
            Plasmoid.configuration.textDefaultsInitialized = true
            // The invokable is present in KConfigPropertyMap; Plasma's QML
            // property type information does not expose it to qmllint.
            // qmllint disable missing-property
            Plasmoid.configuration.writeConfig()
            // qmllint enable missing-property
        }
    }
    fullRepresentation: SystemMonitor {
        Layout.minimumWidth: 390
        Layout.minimumHeight: Math.max(140, minimumCardHeight)
        Layout.preferredWidth: 496
        Layout.preferredHeight: 164
        sensorIds: Plasmoid.configuration.sensorIds
        sensorColumns: Plasmoid.configuration.sensorColumns
        sensorLabels: SensorLabels.parse(Plasmoid.configuration.sensorLabels)
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
        unifiedTextColor: Plasmoid.configuration.unifiedTextColor
        valueColor: Plasmoid.configuration.valueColor
        titleColor: Plasmoid.configuration.titleColor
        detailColor: Plasmoid.configuration.detailColor
        cpuGraphColor: Plasmoid.configuration.cpuGraphColor
        gpuTemperatureGraphColor: Plasmoid.configuration.gpuTemperatureGraphColor
        cpuTemperatureGraphColor: Plasmoid.configuration.cpuTemperatureGraphColor
        chartOutline: Plasmoid.configuration.chartOutline
    }
}

// SPDX-License-Identifier: LGPL-3.0-or-later
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: qsTr("Sensors")
        icon: "utilities-system-monitor"
        source: "ConfigSensors.qml"
    }
    ConfigCategory {
        name: qsTr("Liquid Glass")
        icon: "preferences-desktop-effects"
        source: "ConfigEffects.qml"
    }
    ConfigCategory {
        name: qsTr("Text and charts")
        icon: "preferences-desktop-font"
        source: "ConfigText.qml"
    }
}

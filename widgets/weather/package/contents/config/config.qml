// SPDX-License-Identifier: LGPL-3.0-or-later
import org.kde.plasma.configuration
ConfigModel {
    ConfigCategory { name: qsTr("Location and provider"); icon: "mark-location"; source: "ConfigLocation.qml" }
    ConfigCategory { name: qsTr("Liquid Glass"); icon: "preferences-desktop-effects"; source: "ConfigEffects.qml" }
    ConfigCategory { name: qsTr("Text"); icon: "preferences-desktop-font"; source: "ConfigText.qml" }
}

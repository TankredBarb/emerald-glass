// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Controls as Controls
import QtTest
import org.kde.kirigami as Kirigami
import "../package/contents/ui" as Monitor

Item {
    id: scene
    width: 620
    height: 660
    Kirigami.PageRow {
        id: pages
        anchors.fill: parent
        initialPage: Monitor.ConfigEffects { id: settings }
    }
    TestCase {
        name: "MaterialSettings"
        when: windowShown

        function init() {
            scene.width = 620
            settings.restoreDefaults()
        }
        function test_controls_update_preview_without_saving() {
            let preview = findChild(settings, "effectsPreview")
            verify(preview !== null)
            settings.cfg_opticsEnabled = false
            settings.cfg_refraction = 155
            settings.cfg_dispersion = 70
            settings.cfg_specular = 125
            settings.cfg_hoverGlow = 80
            settings.cfg_blurRadius = 60
            settings.cfg_materialOpacity = 85
            settings.cfg_tintColor = "#ff6688"
            settings.cfg_tintStrength = 75
            settings.cfg_adaptiveContrast = 25
            settings.cfg_cornerRadius = 32
            compare(preview.opticsEnabled, false)
            compare(preview.refraction, 1.55)
            compare(preview.dispersion, 0.7)
            compare(preview.specular, 1.25)
            compare(preview.hoverGlow, 0.8)
            compare(preview.blurRadius, 6)
            compare(preview.materialOpacity, 0.85)
            compare(preview.tintColor, Qt.color("#ff6688"))
            compare(preview.tintStrength, 0.75)
            compare(preview.adaptiveContrast, 0.25)
            compare(preview.cornerRadius, 32)
        }
        function test_restore_first_version_appearance() {
            settings.cfg_refraction = 180
            settings.cfg_materialOpacity = 100
            settings.cfg_specular = 0
            settings.cfg_tintColor = "red"
            settings.cfg_opticsEnabled = false
            settings.restoreDefaults()
            let preview = findChild(settings, "effectsPreview")
            compare(preview.opticsEnabled, true)
            compare(preview.refraction, 1)
            compare(preview.dispersion, 0.25)
            compare(preview.specular, 1)
            compare(preview.hoverGlow, 0)
            compare(preview.blurRadius, 2)
            compare(preview.materialOpacity, 0.4)
            compare(preview.tintColor, Qt.color("#296b75"))
            compare(preview.tintStrength, 0.2)
            compare(preview.adaptiveContrast, 1)
            compare(preview.cornerRadius, 20)
        }
        function test_preview_and_controls_fit_narrow_page() {
            scene.width = 440
            wait(100)
            let contents = findChild(settings, "settingsContents")
            verify(contents.width <= settings.availableWidth)
            let preview = findChild(settings, "effectsPreview")
            verify(preview.width > 300)
            verify(preview.width < settings.width)
        }
    }
}

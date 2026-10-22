// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Window
import QtTest
import "../package/contents/ui" as Monitor

Item {
    id: testScene
    width: 800
    height: 480

    Item {
        id: wall
        anchors.fill: parent
        Rectangle { width: parent.width / 2; height: parent.height; color: "#d03020" }
        Rectangle { x: parent.width / 2; width: parent.width / 2; height: parent.height; color: "#2050d0" }
        Rectangle { id: pattern; x: 35; width: 7; height: parent.height; color: "white"; visible: false }
    }
    Item {
        id: movingParent
        x: 30
        y: 30
        Monitor.GlassCard {
            id: glass
            width: 250
            height: 160
            wallpaper: wall
        }
    }
    SignalSpy {
        id: frameSpy
        target: testScene.Window.window
        signalName: "afterAnimating"
    }
    TestCase {
        name: "DesktopGlass"
        when: windowShown
        function pixel(image, x, y) {
            return image.pixel(Math.round(x * Screen.devicePixelRatio), Math.round(y * Screen.devicePixelRatio))
        }
        function init() {
            movingParent.x = 30
            movingParent.y = 30
            movingParent.rotation = 0
            movingParent.scale = 1
            movingParent.layer.enabled = false
            glass.width = 250
            glass.height = 160
            glass.wallpaper = wall
            glass.opticsEnabled = true
            glass.refraction = 1
            glass.dispersion = 0.25
            glass.specular = 1
            glass.hoverGlow = 0
            glass.blurRadius = 2
            glass.materialOpacity = 0.4
            glass.tintColor = "#296b75"
            glass.tintStrength = 0.2
            glass.adaptiveContrast = 1
            glass.cornerRadius = 20
            wall.children[0].color = "#d03020"
            pattern.visible = false
            pattern.x = 35
            mouseMove(testScene, 799, 479)
            wait(150)
        }
        function neutralMaterial() {
            glass.refraction = 0
            glass.dispersion = 0
            glass.specular = 0
            glass.blurRadius = 0
            glass.materialOpacity = 0
            glass.tintStrength = 0
            glass.adaptiveContrast = 0
        }
        function test_wallpaper_follows_ancestor_movement() {
            let first = pixel(grabImage(testScene), 155, 110)
            verify(first.r > first.b, "Card must sample the red wallpaper region")
            verify(first.r < 0.8, "The glass material must be rendered")
            movingParent.x = 480
            tryCompare(glass, "wallpaperPosition", Qt.point(480, 30))
            wait(150)
            let second = pixel(grabImage(testScene), 605, 110)
            verify(second.b > second.r, "Moved card must sample the blue wallpaper region")
        }
        function test_wallpaper_matches_each_presented_frame_data() {
            return [{tag: "move", transform: false}, {tag: "move_rotate_scale", transform: true}]
        }
        function test_wallpaper_matches_each_presented_frame(data) {
            for (let step = 1; step <= 24; step++) {
                movingParent.x = 30 + step * 9
                movingParent.y = 30 + step % 2 * 17
                if (data.transform) {
                    movingParent.rotation = step * 2
                    movingParent.scale = 1 + step / 120
                }
                verify(waitForRendering(testScene), "A moved card must render a frame")
                const origin = glass.mapToItem(wall, 0, 0)
                const axisX = glass.mapToItem(wall, 1, 0)
                const axisY = glass.mapToItem(wall, 0, 1)
                verify(Math.hypot(glass.wallpaperPosition.x - origin.x,
                    glass.wallpaperPosition.y - origin.y) < 0.001, "Wallpaper must match this frame, without timer lag")
                fuzzyCompare(glass.wallpaperAxisX.x, axisX.x - origin.x, 0.001)
                fuzzyCompare(glass.wallpaperAxisX.y, axisX.y - origin.y, 0.001)
                fuzzyCompare(glass.wallpaperAxisY.x, axisY.x - origin.x, 0.001)
                fuzzyCompare(glass.wallpaperAxisY.y, axisY.y - origin.y, 0.001)
            }
        }
        function test_static_glass_does_not_request_frames() {
            verify(frameSpy.valid)
            wait(120)
            frameSpy.clear()
            wait(200)
            compare(frameSpy.count, 0, "Static glass must not drive the window render loop")
        }
        function test_wallpaper_updates_without_recreating_card() {
            wall.children[0].color = "#2050d0"
            wait(150)
            let sample = pixel(grabImage(testScene), 155, 110)
            verify(sample.b > sample.r)
            wall.children[0].color = "#d03020"
        }
        function test_wallpaper_follows_rotation_and_scale_data() {
            return [
                {tag: "rotation", angle: 35, scale: 1, layer: false},
                {tag: "rotation_with_layer", angle: 35, scale: 1, layer: true},
                {tag: "scaled_rotation", angle: 40, scale: 1.3, layer: false}
            ]
        }
        function test_wallpaper_follows_rotation_and_scale(data) {
            neutralMaterial()
            movingParent.x = 300
            movingParent.y = 140
            movingParent.rotation = data.angle
            movingParent.scale = data.scale
            movingParent.layer.enabled = data.layer
            wait(200)
            const image = grabImage(testScene)
            for (let x = 30; x < glass.width - 20; x += 20) {
                const pos = glass.mapToItem(testScene, x, glass.height / 2)
                if (Math.abs(pos.x - wall.width / 2) < 5) continue
                compare(pixel(image, pos.x, pos.y), Qt.color(pos.x < wall.width / 2 ? "#d03020" : "#2050d0"))
            }
        }
        function test_resize_and_transparent_corners() {
            glass.width = 390
            glass.height = 140
            wait(150)
            let image = grabImage(testScene)
            // grabImage includes the scene behind the item. A transparent
            // corner must leave the original wallpaper pixel untouched.
            compare(pixel(image, 30, 30), Qt.color("#d03020"))
            let center = pixel(image, 225, 100)
            verify(center.r < 0.8 && center.r > center.b)
        }
        function test_missing_wallpaper_has_readable_fallback() {
            glass.wallpaper = null
            wait(100)
            compare(glass.backdropReady, false)
            let sample = pixel(grabImage(testScene), 155, 110)
            verify(sample.r < 0.65 && sample.b < 0.4)
        }
        function test_optics_off_is_neutral() {
            neutralMaterial()
            glass.refraction = 1.8
            glass.dispersion = 1
            glass.specular = 1.5
            glass.hoverGlow = 1.5
            glass.opticsEnabled = false
            wait(100)
            let image = grabImage(testScene)
            compare(pixel(image, 35, 110), Qt.color("#d03020"))
            compare(pixel(image, 155, 110), Qt.color("#d03020"))
        }
        function test_zero_highlights_leave_no_bevel_shading() {
            neutralMaterial()
            wait(100)
            let image = grabImage(testScene)
            compare(pixel(image, 32, 110), Qt.color("#d03020"))
            compare(pixel(image, 36, 110), Qt.color("#d03020"))
        }
        function test_opaque_material_excludes_wallpaper() {
            glass.materialOpacity = 1
            wait(100)
            let first = grabImage(testScene)
            wall.children[0].color = "white"
            wait(100)
            let second = grabImage(testScene)
            compare(pixel(first, 155, 110), pixel(second, 155, 110))
            compare(pixel(first, 35, 110), pixel(second, 35, 110))
        }
        function test_refraction_moves_actual_wallpaper_detail() {
            neutralMaterial()
            pattern.visible = true
            wait(100)
            let first = pixel(grabImage(testScene), 36, 110)
            glass.refraction = 1
            wait(100)
            let second = pixel(grabImage(testScene), 36, 110)
            verify(first.g > second.g + 0.2)
        }
        function test_dispersion_changes_refracted_edge_colors() {
            neutralMaterial()
            pattern.visible = true
            pattern.x = 50
            glass.refraction = 1
            wait(100)
            let first = pixel(grabImage(testScene), 33, 110)
            glass.dispersion = 1
            wait(100)
            let second = pixel(grabImage(testScene), 33, 110)
            verify(Math.abs(first.b - second.b) > 0.1 || Math.abs(first.r - second.r) > 0.1)
        }
        function test_blur_softens_wallpaper_detail() {
            neutralMaterial()
            pattern.visible = true
            pattern.x = 150
            wait(100)
            let first = pixel(grabImage(testScene), 155, 110)
            glass.blurRadius = 10
            wait(100)
            let second = pixel(grabImage(testScene), 155, 110)
            verify(first.g > second.g + 0.1)
        }
        function test_tint_color_and_strength() {
            neutralMaterial()
            wall.children[0].color = "white"
            glass.tintStrength = 1
            glass.tintColor = "red"
            wait(100)
            let first = pixel(grabImage(testScene), 155, 110)
            glass.tintColor = "blue"
            wait(100)
            let second = pixel(grabImage(testScene), 155, 110)
            verify(first.r > first.b + 0.2)
            verify(second.b > second.r + 0.2)
            glass.tintStrength = 0
            wait(100)
            compare(pixel(grabImage(testScene), 155, 110), Qt.color("white"))
        }
        function test_adaptive_contrast_limits_bright_wallpaper() {
            neutralMaterial()
            wall.children[0].color = "white"
            wait(100)
            let first = pixel(grabImage(testScene), 155, 110)
            glass.adaptiveContrast = 1
            wait(100)
            let second = pixel(grabImage(testScene), 155, 110)
            verify(first.r > second.r + 0.5)
        }
        function test_cursor_glow_tracks_pointer() {
            neutralMaterial()
            glass.hoverGlow = 1
            wait(100)
            let first = pixel(grabImage(testScene), 32, 110)
            mouseMove(glass, 2, 80)
            wait(100)
            let second = pixel(grabImage(testScene), 32, 110)
            verify(second.b > first.b + 0.04, "Cursor must brighten the edge independently of highlights")
        }
    }
}

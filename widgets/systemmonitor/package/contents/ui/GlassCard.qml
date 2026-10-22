// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtQuick.Window

Item {
    id: card
    property Item wallpaper
    property bool opticsEnabled: true
    property real refraction: 1.0
    property real dispersion: 0.25
    property real specular: 1.0
    property real hoverGlow: 0.0
    property real blurRadius: 2.0
    property real materialOpacity: 0.40
    property color tintColor: "#296b75"
    property real tintStrength: 0.20
    property real adaptiveContrast: 1.0
    property real cornerRadius: 20
    property point wallpaperPosition: Qt.point(0, 0)
    property point wallpaperAxisX: Qt.point(1, 0)
    property point wallpaperAxisY: Qt.point(0, 1)
    property rect captureRect: Qt.rect(0, 0, 1, 1)
    readonly property int captureMargin: Math.ceil(20 + refraction * 20 + dispersion * 12 + blurRadius)
    readonly property bool backdropReady: wallpaper !== null && wallpaper.width > 0 && wallpaper.height > 0

    function updatePosition() {
        if (backdropReady) {
            wallpaperPosition = mapToItem(wallpaper, 0, 0)
            const x = mapToItem(wallpaper, 1, 0)
            const y = mapToItem(wallpaper, 0, 1)
            wallpaperAxisX = Qt.point(x.x - wallpaperPosition.x, x.y - wallpaperPosition.y)
            wallpaperAxisY = Qt.point(y.x - wallpaperPosition.x, y.y - wallpaperPosition.y)
            const a = mapToItem(wallpaper, -captureMargin, -captureMargin)
            const b = mapToItem(wallpaper, width + captureMargin, -captureMargin)
            const c = mapToItem(wallpaper, -captureMargin, height + captureMargin)
            const d = mapToItem(wallpaper, width + captureMargin, height + captureMargin)
            const left = Math.floor(Math.min(a.x, b.x, c.x, d.x))
            const top = Math.floor(Math.min(a.y, b.y, c.y, d.y))
            captureRect = Qt.rect(left, top,
                Math.max(1, Math.ceil(Math.max(a.x, b.x, c.x, d.x)) - left),
                Math.max(1, Math.ceil(Math.max(a.y, b.y, c.y, d.y)) - top))
        }
    }

    // Ancestor transforms are not dependencies of mapToItem(). Map them after
    // input/animations and before this frame is synchronized to the renderer.
    // Listening to existing frames does not drive rendering while idle.
    Connections {
        target: card.Window.window
        enabled: card.visible && card.backdropReady
        function onAfterAnimating() { card.updatePosition() }
    }
    onWallpaperChanged: updatePosition()

    HoverHandler { id: hover }

    ShaderEffectSource {
        id: backdrop
        sourceItem: card.wallpaper
        sourceRect: card.captureRect
        textureSize: Qt.size(card.captureRect.width, card.captureRect.height)
        live: card.visible
        hideSource: false
        recursive: false
        visible: false
    }

    Rectangle {
        anchors.fill: parent
        radius: Math.min(card.cornerRadius, width / 2, height / 2)
        color: Qt.rgba(0.035, 0.075, 0.095, card.materialOpacity)
        border.color: card.opticsEnabled ? Qt.rgba(card.tintColor.r, card.tintColor.g, card.tintColor.b, card.specular * 0.4) : "transparent"
        visible: !card.backdropReady || GraphicsInfo.api === GraphicsInfo.Software
    }

    ShaderEffect {
        anchors.fill: parent
        visible: card.backdropReady && GraphicsInfo.api !== GraphicsInfo.Software
        property variant scene: backdrop
        property vector2d cardSize: Qt.vector2d(width, height)
        property vector2d sampleOrigin: Qt.vector2d(
            (card.wallpaperPosition.x - card.captureRect.x) / card.captureRect.width,
            (card.wallpaperPosition.y - card.captureRect.y) / card.captureRect.height)
        property vector2d sampleAxisX: Qt.vector2d(
            card.wallpaperAxisX.x / card.captureRect.width, card.wallpaperAxisX.y / card.captureRect.height)
        property vector2d sampleAxisY: Qt.vector2d(
            card.wallpaperAxisY.x / card.captureRect.width, card.wallpaperAxisY.y / card.captureRect.height)
        property real cornerRadius: Math.min(card.cornerRadius, width / 2, height / 2)
        property real refraction: card.opticsEnabled ? card.refraction : 0
        property real dispersion: card.opticsEnabled ? card.dispersion : 0
        property real specular: card.opticsEnabled ? card.specular : 0
        property real hoverGlow: card.opticsEnabled ? card.hoverGlow : 0
        property real blurRadius: card.blurRadius
        property real materialOpacity: card.materialOpacity
        property color tintColor: card.tintColor
        property real tintStrength: card.tintStrength
        property real adaptiveContrast: card.adaptiveContrast
        property vector2d pointer: hover.hovered ? Qt.vector2d(hover.point.position.x, hover.point.position.y) : Qt.vector2d(-1000, -1000)
        fragmentShader: "glass.frag.qsb"
    }
}

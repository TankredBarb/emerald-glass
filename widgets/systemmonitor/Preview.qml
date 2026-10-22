// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick

Rectangle {
    width: 780
    height: 380
    color: "#243f50"
    Image {
        id: wallpaper
        anchors.fill: parent
        source: previewWallpaper
        fillMode: Image.PreserveAspectCrop
    }
    Loader {
        id: card
        width: 496
        height: 164
        anchors.centerIn: parent
        source: monitorUi + "/SystemMonitor.qml"
        onLoaded: item.wallpaper = wallpaper
    }
}

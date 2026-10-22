// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import QtTest
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.private.containmentlayoutmanager as ContainmentLayoutManager
import "../package/contents/ui" as Monitor

Item {
    id: scene
    width: 1000
    height: 700
    Rectangle { id: wall; anchors.fill: parent; color: "#a05030" }
    PlasmaCore.Action { id: dummyAction }
    QtObject {
        id: dummyPlasmoid
        property bool hasConfigurationInterface: false
        property int backgroundHints: 0
        function internalAction(name) { return dummyAction }
    }
    QtObject { id: applet; property QtObject plasmoid: dummyPlasmoid }
    ContainmentLayoutManager.AppletsLayout {
        anchors.fill: parent
        ContainmentLayoutManager.ItemContainer {
            id: appletContainer
            x: 200
            y: 250
            width: 400
            height: 164
            editModeCondition: ContainmentLayoutManager.ItemContainer.Manual
            configOverlaySource: desktopOverlayUrl
            contentItem: Item {
                Monitor.DesktopRotationFix { }
                Monitor.SystemMonitor { anchors.fill: parent; wallpaper: wall }
            }
        }
    }
    TestCase {
        name: "NativeRotation"
        when: windowShown
        function findHandle(item) {
            if (item.startRotation !== undefined) return item
            for (const child of item.children) {
                const handle = findHandle(child)
                if (handle) return handle
            }
            return null
        }
        function test_repeat_rotate() {
            if (desktopOverlayUrl.toString().length === 0) skip("Plasma desktop edit overlay is not installed")
            appletContainer.editMode = true
            tryVerify(() => appletContainer.configOverlayItem !== null)
            wait(400)
            const handle = findHandle(appletContainer.configOverlayItem)
            verify(handle !== null)
            for (let i = 0; i < 3; i++) {
                const before = appletContainer.rotation
                const start = handle.mapToItem(scene, handle.width / 2, handle.height / 2)
                mouseMove(scene, start.x, start.y, 50)
                mousePress(scene, start.x, start.y)
                verify(handle.pressed, "Repeated press must reach the rotate handle")
                for (let step = 1; step <= 10; step++) {
                    mouseMove(scene, start.x - step * 3, start.y - step * 5, 30)
                }
                mouseRelease(scene, start.x - 30, start.y - 50)
                wait(100)
                verify(Math.abs(appletContainer.rotation - before) > 5)
                compare(appletContainer.width, 400)
                compare(appletContainer.height, 164)
            }
        }
    }
}

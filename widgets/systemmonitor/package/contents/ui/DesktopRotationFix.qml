// SPDX-License-Identifier: LGPL-3.0-or-later
import QtQuick
import org.kde.plasma.private.containmentlayoutmanager as ContainmentLayoutManager

Item {
    id: fix
    visible: false
    readonly property ContainmentLayoutManager.ItemContainer container: findContainer(parent)

    function findContainer(item: Item): ContainmentLayoutManager.ItemContainer {
        while (item) {
            if (item instanceof ContainmentLayoutManager.ItemContainer) return item
            item = item.parent
        }
        return null
    }

    function suppressRotationTip(item) {
        if (!item) return
        // Plasma's tooltip is placed above the unrotated button. After a
        // rotation it can cover the button and intercept the next press.
        // Keep this workaround local to this applet's edit overlay.
        if ("toolTip" in item && item.icon?.name === "object-rotate-left-symbolic") {
            item.toolTip = ""
            return
        }
        for (const child of item.children) suppressRotationTip(child)
    }

    Connections {
        target: fix.container
        function onConfigOverlayItemChanged() { fix.suppressRotationTip(fix.container.configOverlayItem) }
    }
    onContainerChanged: suppressRotationTip(container?.configOverlayItem ?? null)
}

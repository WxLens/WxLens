// SPDX-License-Identifier: MIT
import QtQuick

// Warning polygons and placefile vector items (lines, triangles, polygons) are drawn by the
// pane's C++ render::OverlayLayer inside the map itself - this item only hands it the manager.
// That layer owns filtering ("nearby only", per-pane override), styling and geometry, and is
// rebuilt only when overlay data or those options change, so camera movement costs nothing here.
// What stays in QML are the placefile items that are not vector geometry: icon sprites and text
// labels, which are few, positioned per camera tick like every other geo-anchored QML layer.
Item {
    id: root
    property var paneController
    property var manager
    property int cameraTick: 0

    Binding {
        target: root.paneController
        property: "overlayManager"
        value: root.manager
        when: root.paneController !== null && root.paneController !== undefined
    }

    function itemsOfKind(kind) {
        var result = []
        if (!root.manager || !root.manager.placefilesVisible) return result
        const items = root.manager.placefileItems
        for (var i = 0; i < items.length; ++i)
            if (items[i].kind === kind) result.push(items[i])
        return result
    }

    // Filtered once per data change, so the Repeaters below create delegates only for the items
    // they draw rather than one hidden delegate per line/polygon.
    readonly property var iconItems: root.itemsOfKind("icon")
    readonly property var textItems: root.itemsOfKind("text")

    function anchorFor(item) {
        const ignored = root.cameraTick
        return root.paneController
            ? root.paneController.pixelForCoordinate(item.latitude, item.longitude)
            : Qt.point(0, 0)
    }

    // Placefile icon sheets are raster resources; sourceClipRect selects the requested sprite cell.
    Repeater {
        model: root.iconItems
        delegate: Image {
            required property var modelData
            readonly property point anchor: root.anchorFor(modelData)
            x: anchor.x - (modelData.hotX || 0); y: anchor.y - (modelData.hotY || 0)
            width: modelData.width || 0; height: modelData.height || 0
            source: modelData.source
            sourceClipRect: Qt.rect((modelData.iconNumber || 0) * width, 0, width, height)
            rotation: modelData.angle || 0
            smooth: true
        }
    }

    Repeater {
        model: root.textItems
        delegate: Text {
            required property var modelData
            readonly property point anchor: root.anchorFor(modelData)
            // Anchored at the baseline's left end, as the Canvas fillText it replaces was.
            x: anchor.x; y: anchor.y - baselineOffset
            text: modelData.text
            color: modelData.color
            font.family: "sans-serif"
            font.pixelSize: 12
        }
    }
}

// SPDX-License-Identifier: MIT
import QtQuick
import MapLibre 4.0

// One pane's map surface, bound to a nimbus::panes::PaneController (docs/ROADMAP.md §4.6).
// Everything pane-specific comes from `paneController` - this item holds no radar/site state of
// its own, and several of these exist at once inside PaneGrid.
//
// Base map: OpenFreeMap (docs/data-sources.md), free/unlimited/no-API-key OSM vector tiles - both
// dark and light styles come from the same tile source, so switching is just a style URL swap.
// darkMode is hardcoded here for now; it should follow ThemeManager's active theme once that
// exists (slice 10), not stay a local property. A future "map provider" setting can let a user
// swap in their own style URL/key (e.g. MapTiler) for more detail - OpenFreeMap is the
// no-signup-required default, not the only option.
Rectangle {
    id: root
    color: "#000000"

    // The nimbus::panes::PaneController this pane renders.
    required property var paneController

    // Shrinking the grid destroys PaneControllers while their delegates are still being torn
    // down, so QML sees `paneController` go null for a moment and every binding/handler below
    // must tolerate it. Without this guard the teardown throws a burst of
    // "TypeError: Cannot read property ... of null" on every grid shrink.
    readonly property bool hasController: paneController !== null &&
                                          paneController !== undefined

    // Whether to show the per-pane identification label (only useful once the grid has more than
    // one pane). Passed in rather than read from a global so this item stays self-contained.
    property bool showLabel: false

    // Set while applying an incoming synced camera, to stop the map's resulting change
    // notification from being reported back as user input. See the Connections block below.
    property bool applyingSync: false

    readonly property bool darkMode: true
    readonly property string mapStyle: darkMode
        ? "https://tiles.openfreemap.org/styles/dark"
        : "https://tiles.openfreemap.org/styles/positron"

    // The vendored example this was ported from uses a bare Math.pow(2, angleDelta/120) - a full
    // 2x/0.5x zoom per single wheel notch, which is too coarse for precise navigation (user
    // feedback during Phase 1 slice 2). This scales that down to roughly a 19% zoom change per
    // notch instead; tune here if it still feels too fast/slow.
    readonly property real wheelZoomSensitivity: 0.25

    MapLibre {
        id: map
        anchors.fill: parent
        focus: true

        style: root.mapStyle

        // Seeded imperatively rather than bound, because the camera is written back below:
        // assigning to a property in QML replaces any binding on it, so a binding here plus the
        // write-back would silently break after the first user pan. Slice 5's sync will drive the
        // other direction explicitly, from PaneController's cameraChanged.
        Component.onCompleted: {
            if (!root.hasController) {
                return
            }
            map.zoomLevel = root.paneController.zoom
            map.coordinate = [root.paneController.centerLatitude,
                              root.paneController.centerLongitude]
        }

        // The controller is where camera state lives (§4.6), so slice 5's per-channel sync and
        // the persistence schema both have a single source of truth. Panes are fully independent
        // this slice, so there is no propagation to guard against yet - that guard (§4.2) arrives
        // with sync itself. MapQuickItem exposes only `coordinate` and `zoomLevel`; bearing/pitch
        // have no QML property to observe yet, so PaneController's stay at their defaults.
        onCoordinateChanged: {
            if (!root.hasController || root.applyingSync) {
                return
            }
            root.paneController.setCenter(map.coordinate[0], map.coordinate[1])
        }
        onZoomLevelChanged: {
            if (root.hasController && !root.applyingSync) {
                root.paneController.zoom = map.zoomLevel
            }
        }

        // The other direction: when this pane's camera is changed by the sync coordinator, move
        // the map to match. Bound to cameraSynced, NOT cameraChanged - cameraChanged also fires
        // for this pane's own gestures, and re-applying the controller's camera during a local
        // gesture fights it. Zoom-about-cursor shifts the centre as part of zooming, so snapping
        // the centre back mid-gesture turned wheel-zoom into a sideways slide whose direction
        // depended on cursor position. applyingSync then suppresses the write-back above, so an
        // applied sync is not reported straight back as though the user had panned here.
        Connections {
            target: root.hasController ? root.paneController : null

            function onCameraSynced() {
                root.applyingSync = true
                if (map.zoomLevel !== root.paneController.zoom) {
                    map.zoomLevel = root.paneController.zoom
                }
                if (map.coordinate[0] !== root.paneController.centerLatitude ||
                    map.coordinate[1] !== root.paneController.centerLongitude) {
                    map.coordinate = [root.paneController.centerLatitude,
                                      root.paneController.centerLongitude]
                }
                root.applyingSync = false
            }
        }

        // Registers this pane's Visualization Layer(s). Must wait for styleLoaded, not just
        // mapReady - addCustomLayer() calls made before the style has actually loaded are
        // silently dropped (confirmed: matches the legacy app's own MapWidget::mapChanged, which
        // gates its equivalent AddLayers() call the same way). mapLibreMap()/mapReady()/
        // styleLoaded() are not upstream - see
        // external/patches/0005-mln-qt-expose-map-object.patch.
        onStyleLoaded: {
            if (root.hasController) {
                root.paneController.attachLayers(map.mapLibreMap())
            }
        }

        PinchHandler {
            id: pinch
            target: null
            onScaleChanged: (delta) => {
                map.scale(delta, pinch.centroid.position)
            }
            grabPermissions: PointerHandler.TakeOverForbidden
        }

        DragHandler {
            id: drag
            target: null
            onTranslationChanged: (delta) => map.pan(delta)
        }

        WheelHandler {
            id: wheel
            acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                            ? PointerDevice.Mouse | PointerDevice.TouchPad
                            : PointerDevice.Mouse
            onWheel: (event) => {
                map.scale(Math.pow(2.0, (event.angleDelta.y / 120) * root.wheelZoomSensitivity),
                          wheel.point.position)
            }
        }
    }

    // Quick sync control (docs/ROADMAP.md §4.5): linking must be reachable without opening
    // Settings. Cycles this pane through camera-link groups A and B and back to independent.
    // Deliberately minimal - it drives the general per-channel model underneath, which supports
    // combinations (e.g. shared location with independent zoom) this control does not yet expose.
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 4
        visible: root.showLabel

        Rectangle {
            width: 62
            height: 22
            radius: 4
            color: linked ? "#2b3a4d" : "#1a1f26"
            border.color: linked ? "#4a7ab0" : "#2f3742"
            border.width: 1
            opacity: 0.92

            // Referencing syncRevision is what makes this binding re-evaluate: group membership
            // is read through a method, which QML cannot track for staleness on its own.
            readonly property int group:
                root.hasController && typeof paneGridModel !== "undefined"
                    ? (paneGridModel.syncRevision,
                       paneGridModel.cameraSyncGroup(root.paneController.paneId))
                    : 0
            readonly property bool linked: group !== 0

            Text {
                anchors.centerIn: parent
                text: parent.linked
                    ? "Link " + String.fromCharCode(64 + parent.group)
                    : "Unlinked"
                color: parent.linked ? "#dce6f2" : "#8d99a8"
                font.pixelSize: 10
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                // none -> A -> B -> none. Two groups is enough to demonstrate that groups are
                // independent of one another, which one group alone would not show.
                onClicked: paneGridModel.setCameraSyncGroup(
                    root.paneController.paneId, (parent.group + 1) % 3)
            }
        }
    }

    // Minimal per-pane identification while the grid has more than one pane. Full pane chrome
    // (site/product pickers) still lands later.
    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 6
        visible: root.showLabel
        text: root.hasController
            ? root.paneController.sourceKey + " " + root.paneController.productName
            : ""
        font.pixelSize: 11
        color: "#c8d0d8"
        style: Text.Outline
        styleColor: "#00000090"
    }

    // OSM's ODbL and the OpenMapTiles schema both require attribution; MapLibre Native Qt's
    // Quick item has no built-in attribution control (unlike MapLibre GL JS), so this is added
    // by hand. Keep this text (or equivalent credit) on any pane using OpenFreeMap-sourced tiles.
    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        text: "© OpenStreetMap contributors © OpenMapTiles"
        font.pixelSize: 10
        color: "#c8d0d8"
        style: Text.Outline
        styleColor: "#00000090"
    }
}

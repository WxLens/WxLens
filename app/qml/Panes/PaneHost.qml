// SPDX-License-Identifier: MIT
import QtQuick
import MapLibre 4.0

// Phase 1 slice 1: proves the Qt Quick/QML -> MapLibre Native Qt rendering seam end-to-end
// (docs/ROADMAP.md §0.2 "prove the rendering seam early") with one bare, hardcoded map surface -
// no PaneGridModel/PaneController (slice 4), no radar data (slice 2+), no per-channel sync
// (slice 5).
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
        zoomLevel: 4
        coordinate: [39.8, -98.6] // CONUS center, matches the app's initial radar-viewer use case

        // Registers the radar reflectivity sweep custom layer (nimbus::render::RadarSweepLayer,
        // docs/ROADMAP.md §7 Phase 1 slice 3). Must wait for styleLoaded, not just mapReady -
        // addCustomLayer() calls made before the style has actually loaded are silently dropped
        // (confirmed: matches the legacy app's own MapWidget::mapChanged, which gates its
        // equivalent AddLayers() call the same way). mapLibreMap()/mapReady()/styleLoaded() are
        // not upstream - see external/patches/0005-mln-qt-expose-map-object.patch.
        onStyleLoaded: {
            if (typeof radarLayerController !== "undefined" &&
                typeof radarSiteLatitude !== "undefined") {
                radarLayerController.attachRadarSweep(
                    map.mapLibreMap(), radarStatus.siteId, radarSiteLatitude, radarSiteLongitude)
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

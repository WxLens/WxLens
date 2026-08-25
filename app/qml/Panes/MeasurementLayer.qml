// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes

// The in-progress measurement (docs/ROADMAP.md §4.4), drawn per pane.
//
// This is tier-1 Temporary state (§4.3): it belongs to the measurement tool, never to
// MapObjectStore, and disappears when the interaction ends. That separation is why it is a
// distinct layer from MapObjectsLayer rather than a special object type inside it.
//
// The measurement itself lives in C++ (nimbus::objects::MeasurementController) including all the
// geodesic math; this file only projects its coordinates and draws them.
Item {
    id: root

    required property var paneController
    required property var controller

    // Bumped by PaneHost on every camera change, so the in-progress measurement re-projects while
    // panning and zooming exactly like stored objects do.
    property int cameraTick: 0

    readonly property bool ready:
        paneController !== null && paneController !== undefined &&
        controller !== null && controller !== undefined &&
        // Only the pane that owns the in-progress measurement draws it. The controller is shared
        // by every pane, so without this check a measurement started in one pane was drawn in
        // all of them, through cameras it was never picked against.
        paneController.paneId === controller.activePaneId

    // Flat {lat, lon, ...} from the controller, including the live cursor vertex, projected here.
    readonly property var vertexPixels: {
        if (!ready) {
            return []
        }
        root.cameraTick
        var flat = controller.points
        var pts = []
        for (var i = 0; i + 1 < flat.length; i += 2) {
            pts.push(paneController.pixelForCoordinate(flat[i], flat[i + 1]))
        }
        return pts
    }

    // Live range ring: a circle at the measured distance about the origin, so the range can be
    // read off at any azimuth without having to hold the cursor exactly over the thing being
    // measured. Only for a single segment - a ring about the start of a multi-leg path would
    // describe a distance that path never had.
    //
    // Sampled as a true geodesic circle exactly as MapObjectsLayer draws a pinned range ring, not
    // as a screen-space circle: Mercator stretches with latitude, so a pixel circle would be
    // visibly wrong at wide zooms and away from the equator.
    readonly property bool showRing: root.ready && root.vertexPixels.length === 2 &&
                                     root.controller.segments.length === 1

    readonly property var ringPixels: {
        if (!root.showRing) {
            return []
        }
        root.cameraTick
        var flat = root.controller.points
        var radius = root.controller.segments[0].distanceMeters
        if (radius <= 0) {
            return []
        }
        var pts = []
        var steps = 72
        for (var i = 0; i <= steps; ++i) {
            var geo = root.paneController.coordinateAtOffset(
                flat[0], flat[1], (360.0 / steps) * i, radius)
            pts.push(root.paneController.pixelForCoordinate(geo[0], geo[1]))
        }
        return pts
    }

    Shape {
        visible: root.showRing && root.ringPixels.length > 0
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Qt.rgba(themeManager.measurementAccent.r,
                                 themeManager.measurementAccent.g,
                                 themeManager.measurementAccent.b, 0.6)
            strokeWidth: 1.5
            // A barely-there fill, so the ring reads as an area at a glance without washing out
            // the reflectivity it is drawn over - the data underneath is the point of measuring.
            fillColor: Qt.rgba(themeManager.measurementAccent.r,
                               themeManager.measurementAccent.g,
                               themeManager.measurementAccent.b, 0.06)

            PathPolyline {
                path: root.ringPixels
            }
        }
    }

    Shape {
        visible: root.vertexPixels.length > 1
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: themeManager.measurementAccent
            strokeWidth: 2
            fillColor: "transparent"
            // Dashed, so an unfinished measurement reads as provisional rather than as something
            // already committed to the map.
            strokeStyle: ShapePath.DashLine
            dashPattern: [5, 4]

            PathPolyline {
                path: root.vertexPixels
            }
        }
    }

    Repeater {
        model: root.vertexPixels

        delegate: Rectangle {
            required property var modelData
            required property int index

            x: modelData.x - 4
            y: modelData.y - 4
            width: 8
            height: 8
            radius: 4
            color: index === 0 ? themeManager.measurementAccent : themeManager.background
            border.color: themeManager.measurementAccent
            border.width: 2
        }
    }

    // Per-segment distance, placed at each segment's midpoint. This is the detail that makes a
    // multi-leg path readable while drawing it, rather than only showing a total.
    Repeater {
        model: root.ready ? root.controller.segments : []

        delegate: Text {
            required property var modelData
            required property int index

            visible: index + 1 < root.vertexPixels.length
            x: visible ? (root.vertexPixels[index].x + root.vertexPixels[index + 1].x) / 2 + 6 : 0
            y: visible ? (root.vertexPixels[index].y + root.vertexPixels[index + 1].y) / 2 - 8 : 0
            // Distance and bearing together on a single-segment measurement. Splitting them
            // across two places would mean looking twice to answer one question.
            text: {
                if (!root.ready) {
                    return ""
                }
                const distance = root.controller.formatDistance(modelData.distanceMeters)
                if (root.controller.segments.length > 1) {
                    return distance
                }
                // Three digits, zero-padded: 048° rather than 48°, which is how a bearing is
                // written everywhere else in meteorology.
                const bearing = Math.round(modelData.bearingDegrees) % 360
                return distance + "  ·  " + ("00" + bearing).slice(-3) + "°"
            }
            color: themeManager.measurementAccent
            font.pixelSize: 10
            style: Text.Outline
            styleColor: "#c0000000"
        }
    }
}

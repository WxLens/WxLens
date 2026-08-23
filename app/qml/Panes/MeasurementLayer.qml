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
        controller !== null && controller !== undefined

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

    Shape {
        visible: root.vertexPixels.length > 1
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "#ffd166"
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
            color: index === 0 ? "#ffd166" : "#0d1116"
            border.color: "#ffd166"
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
            text: root.ready ? root.controller.formatDistance(modelData.distanceMeters) : ""
            color: "#ffd166"
            font.pixelSize: 10
            style: Text.Outline
            styleColor: "#000000c0"
        }
    }
}

// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes

// The User Analysis Layer (docs/ROADMAP.md §4.3): markers, drawings and range rings, composited
// above whatever meteorological renderer the pane is showing and independent of it.
//
// Rendered as Qt Quick items rather than an OpenGL custom layer, deliberately: these are a
// handful of vector shapes with text labels per pane, where Qt Quick gives crisp text, hit
// testing and styling for free, and the radar renderer's shader machinery would buy nothing. If
// object counts ever reach the thousands (dense placefiles, say), this is the piece to move to a
// GL layer - the store and scope resolution behind it would not change.
//
// Objects are stored in geographic coordinates and projected here, every frame the camera moves,
// so they stay anchored as each pane pans and zooms independently.
Item {
    id: root

    required property var paneController
    required property var objectStore

    // Re-projects when the camera moves or the store changes. Reading both revision counters
    // makes this binding depend on them, since the underlying lookups are method calls that QML
    // cannot track for staleness on its own.
    readonly property var objects: {
        if (!paneController || !objectStore) {
            return []
        }
        objectStore.revision
        root.cameraTick
        return objectStore.objectsForPane(paneController)
    }

    // Bumped on every camera change to force re-projection of already-visible objects.
    property int cameraTick: 0

    Connections {
        target: root.paneController ? root.paneController : null
        function onCameraChanged() { root.cameraTick++ }
    }

    Repeater {
        model: root.objects

        delegate: Item {
            id: objectItem
            required property var modelData

            anchors.fill: parent

            readonly property int objectType: modelData.objectType

            // Reading cameraTick is what re-projects this on every camera change. Without it the
            // binding evaluates once and the object sticks to its original *pixel*, drifting
            // across the map as you pan and zoom - pixelForCoordinate is a method call, and QML
            // cannot know its result went stale. Every projected value in this file needs the
            // same dependency.
            readonly property point anchorPixel: {
                root.cameraTick
                return root.paneController.pixelForCoordinate(modelData.latitudes[0],
                                                              modelData.longitudes[0])
            }

            // 0 = Marker, 3 = RangeRing (nimbus::objects::MapObjectType)
            readonly property bool isMarker: objectType === 0
            readonly property bool isRangeRing: objectType === 3
            // 1 = Line, 5 = Measurement: both draw as a projected polyline through their vertices.
            readonly property bool isPath: objectType === 1 || objectType === 5

            readonly property var pathPixels: {
                if (!isPath) {
                    return []
                }
                root.cameraTick
                var pts = []
                for (var i = 0; i < modelData.latitudes.length; ++i) {
                    pts.push(root.paneController.pixelForCoordinate(modelData.latitudes[i],
                                                                    modelData.longitudes[i]))
                }
                return pts
            }

            Shape {
                visible: objectItem.isPath
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: objectItem.modelData.color
                    strokeWidth: 2
                    fillColor: "transparent"

                    PathPolyline {
                        path: objectItem.pathPixels
                    }
                }
            }

            // Vertex handles, so a pinned measurement's endpoints stay identifiable against busy
            // reflectivity rather than disappearing into it.
            Repeater {
                model: objectItem.isPath ? objectItem.pathPixels : []

                delegate: Rectangle {
                    required property var modelData
                    x: modelData.x - 3
                    y: modelData.y - 3
                    width: 6
                    height: 6
                    radius: 3
                    color: objectItem.modelData.color
                    border.color: "#101418"
                    border.width: 1
                }
            }

            // A range ring is a true geodesic circle: sample points at a fixed ground distance
            // and project each one, rather than drawing a screen-space circle around the centre.
            // Mercator stretches north-south with latitude, so a fixed pixel radius would be
            // visibly wrong away from the equator and at wide zooms.
            readonly property var ringPixels: {
                if (!isRangeRing) {
                    return []
                }
                root.cameraTick
                var pts = []
                var steps = 72
                for (var i = 0; i <= steps; ++i) {
                    var bearing = (360.0 / steps) * i
                    var geo = root.paneController.coordinateAtOffset(
                        modelData.latitudes[0], modelData.longitudes[0],
                        bearing, modelData.radiusMeters)
                    pts.push(root.paneController.pixelForCoordinate(geo[0], geo[1]))
                }
                return pts
            }

            Shape {
                visible: objectItem.isRangeRing
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: objectItem.modelData.color
                    strokeWidth: 1.5
                    fillColor: "transparent"

                    PathPolyline {
                        path: objectItem.ringPixels
                    }
                }
            }

            // Marker: a simple dot with a halo so it stays readable over both bright reflectivity
            // and the dark basemap.
            Rectangle {
                visible: objectItem.isMarker
                x: objectItem.anchorPixel.x - width / 2
                y: objectItem.anchorPixel.y - height / 2
                width: 12
                height: 12
                radius: 6
                color: objectItem.modelData.color
                border.color: "#101418"
                border.width: 2
            }

            // Paths label at their final vertex, where the user finished, rather than at the
            // start where the label would sit under the rest of the line.
            readonly property point labelPixel:
                objectItem.isPath && objectItem.pathPixels.length > 0
                    ? objectItem.pathPixels[objectItem.pathPixels.length - 1]
                    : objectItem.anchorPixel

            Text {
                visible: objectItem.modelData.label !== ""
                x: objectItem.labelPixel.x + 10
                y: objectItem.labelPixel.y - 8
                text: objectItem.modelData.label
                color: "#e8edf2"
                font.pixelSize: 11
                style: Text.Outline
                styleColor: "#000000c0"
            }
        }
    }
}

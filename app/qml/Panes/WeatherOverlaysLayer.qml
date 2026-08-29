// SPDX-License-Identifier: MIT
import QtQuick

Item {
    id: root
    property var paneController
    property var manager
    property int cameraTick: 0

    function project(coordinates) {
        const ignored = root.cameraTick
        var points = []
        if (!root.paneController) return points
        for (var i = 0; i + 1 < coordinates.length; i += 2)
            points.push(root.paneController.pixelForCoordinate(coordinates[i], coordinates[i + 1]))
        return points
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.manager || !root.paneController) return

            function path(points, close) {
                if (points.length === 0) return false
                ctx.beginPath(); ctx.moveTo(points[0].x, points[0].y)
                for (var i = 1; i < points.length; ++i) ctx.lineTo(points[i].x, points[i].y)
                if (close) ctx.closePath()
                return true
            }

            if (root.manager.warningsVisible) {
                const warnings = root.manager.warningPolygons
                for (var w = 0; w < warnings.length; ++w) {
                    if (!path(root.project(warnings[w].coordinates), true)) continue
                    ctx.globalAlpha = 0.12; ctx.fillStyle = warnings[w].color; ctx.fill()
                    ctx.globalAlpha = 0.95; ctx.strokeStyle = warnings[w].color
                    ctx.lineWidth = 3; ctx.stroke()
                }
            }

            if (root.manager.placefilesVisible) {
                const items = root.manager.placefileItems
                for (var n = 0; n < items.length; ++n) {
                    const item = items[n]
                    ctx.globalAlpha = 1
                    if (item.kind === "line") {
                        if (path(root.project(item.coordinates), false)) {
                            ctx.strokeStyle = item.color; ctx.lineWidth = item.width; ctx.stroke()
                        }
                    } else if (item.kind === "triangles") {
                        const trianglePoints = root.project(item.coordinates)
                        for (var t = 0; t + 2 < trianglePoints.length; t += 3) {
                            if (path([trianglePoints[t], trianglePoints[t + 1], trianglePoints[t + 2]], true)) {
                                ctx.fillStyle = item.color; ctx.fill()
                            }
                        }
                    } else if (item.kind === "polygon") {
                        for (var c = 0; c < item.contours.length; ++c) {
                            if (path(root.project(item.contours[c]), true)) {
                                ctx.globalAlpha = 0.35; ctx.fillStyle = item.color; ctx.fill()
                                ctx.globalAlpha = 0.9; ctx.strokeStyle = item.color; ctx.lineWidth = 1; ctx.stroke()
                            }
                        }
                    } else if (item.kind === "text") {
                        const pixel = root.paneController.pixelForCoordinate(item.latitude, item.longitude)
                        ctx.fillStyle = item.color; ctx.font = "12px sans-serif"
                        ctx.fillText(item.text, pixel.x, pixel.y)
                    }
                }
            }
            ctx.globalAlpha = 1
        }
    }

    // Placefile icon sheets are raster resources, so they stay Image items while the dense vector
    // primitives share the canvas above. sourceClipRect selects the requested sprite cell.
    Repeater {
        model: root.manager && root.manager.placefilesVisible ? root.manager.placefileItems : []
        delegate: Image {
            required property var modelData
            visible: modelData.kind === "icon"
            readonly property point anchor: visible && root.paneController
                ? root.paneController.pixelForCoordinate(modelData.latitude, modelData.longitude)
                : Qt.point(0, 0)
            x: anchor.x - (modelData.hotX || 0); y: anchor.y - (modelData.hotY || 0)
            width: modelData.width || 0; height: modelData.height || 0
            source: visible ? modelData.source : ""
            sourceClipRect: Qt.rect((modelData.iconNumber || 0) * width, 0, width, height)
            rotation: modelData.angle || 0
            smooth: true
        }
    }

    Connections {
        target: root.manager
        function onWarningsChanged() { canvas.requestPaint() }
        function onPlacefilesChanged() { canvas.requestPaint() }
        function onWarningsVisibleChanged() { canvas.requestPaint() }
        function onPlacefilesVisibleChanged() { canvas.requestPaint() }
    }
    onCameraTickChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}

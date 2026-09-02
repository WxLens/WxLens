// SPDX-License-Identifier: MIT
import QtQuick

// Product-owned meteorological graphics. These are intentionally separate from MapObjectsLayer:
// a storm track belongs to the selected radar product/time and is not a user-created object.
Item {
    id: root
    required property var paneController
    property int cameraTick: 0
    // Do not participate in pointer delivery unless there is actually a selectable overlay.
    // An always-enabled TapHandler here sat above pane linking and every drawing MouseArea.
    enabled: paneController && paneController.productOverlays.length > 0

    function selectStormAt(x, y) {
        if (!root.paneController) return
        const overlays = root.paneController.productOverlays
        var nearestId = ""
        var nearestDistance = 14
        for (var i = 0; i < overlays.length; ++i) {
            const item = overlays[i]
            if (!item.stormId) continue
            for (var p = 0; p < item.points.length; ++p) {
                const point = root.paneController.pixelForCoordinate(
                    item.points[p].latitude, item.points[p].longitude)
                const dx = point.x - x
                const dy = point.y - y
                const distance = Math.sqrt(dx * dx + dy * dy)
                if (distance < nearestDistance) {
                    nearestDistance = distance
                    nearestId = item.stormId
                }
            }
        }
        if (nearestId !== "") root.paneController.selectStorm(nearestId)
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.WithinBounds
        onTapped: (eventPoint) => root.selectStormAt(eventPoint.position.x, eventPoint.position.y)
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative
        onPaint: {
            const ignored = root.cameraTick
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.paneController) return
            const overlays = root.paneController.productOverlays
            for (var i = 0; i < overlays.length; ++i) {
                const item = overlays[i]
                const points = []
                for (var p = 0; p < item.points.length; ++p)
                    points.push(root.paneController.pixelForCoordinate(
                                    item.points[p].latitude, item.points[p].longitude))
                if (points.length === 0) continue

                const selected = item.stormId !== "" &&
                                 item.stormId === root.paneController.selectedStorm
                ctx.globalAlpha = item.forecast ? 0.65 : 0.95
                ctx.strokeStyle = selected ? "#ffff45" : item.past ? "#9aa4b2" : "#ff9f1c"
                ctx.fillStyle = ctx.strokeStyle
                ctx.lineWidth = selected ? 3 : 2
                if (points.length > 1) {
                    ctx.beginPath(); ctx.moveTo(points[0].x, points[0].y)
                    for (var j = 1; j < points.length; ++j) ctx.lineTo(points[j].x, points[j].y)
                    ctx.stroke()
                } else {
                    ctx.beginPath(); ctx.arc(points[0].x, points[0].y,
                                             selected ? 7 : 5, 0, Math.PI * 2); ctx.stroke()
                }
                const label = item.stormId || item.label
                if (label) {
                    ctx.font = "bold 11px sans-serif"
                    ctx.fillText(label, points[0].x + 7, points[0].y - 7)
                }
            }
            ctx.globalAlpha = 1
        }
    }

    Connections {
        target: root.paneController
        function onProductDetailsChanged() { canvas.requestPaint() }
        function onSelectedStormChanged() { canvas.requestPaint() }
    }
    onCameraTickChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}

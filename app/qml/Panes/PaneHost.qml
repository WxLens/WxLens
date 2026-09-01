// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Shapes
import WxLens.App
import MapLibre 4.0

// One pane's map surface, bound to a wxlens::panes::PaneController (docs/ROADMAP.md §4.6).
// Everything pane-specific comes from `paneController` - this item holds no radar/site state of
// its own, and several of these exist at once inside PaneGrid.
//
// Base map: OpenFreeMap (docs/data-sources.md), free/unlimited/no-API-key OSM vector tiles - both
// dark and light styles come from the same tile source, so switching is just a style URL swap.
// A future "map provider" setting can let a user
// swap in their own style URL/key (e.g. MapTiler) for more detail - OpenFreeMap is the
// no-signup-required default, not the only option.
Rectangle {
    id: root
    color: "#000000"

    // The wxlens::panes::PaneController this pane renders.
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

    // A control inside this pane asked to open Settings at a specific section (§4.5). Forwarded
    // rather than handled here: a pane does not own the settings surface, and several panes exist.
    signal configureRequested(string sectionId)
    property bool productBrowserOpen: false

    // Which interaction, if any, is currently claiming clicks on this pane. Measurement takes
    // precedence so the two tool families can never both act on one click.
    readonly property bool measuringActive:
        typeof measurementTool !== "undefined" && measurementTool !== null &&
        measurementTool.mode !== 0
    readonly property bool placementActive:
        !measuringActive && typeof objectTools !== "undefined" && objectTools !== null &&
        objectTools.activeTool !== 0

    // AppSettings::MapTheme: 0 follows chrome (the default), 1 forces dark, 2 forces light.
    readonly property bool darkMode: appSettings.mapTheme === 1 ||
                                     (appSettings.mapTheme === 0 && themeManager.dark)
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

        // MapLibre consumes its initial style binding while loading. Drive subsequent changes
        // explicitly so dark -> light -> dark cannot leave the first reloaded style in place.
        Connections {
            target: themeManager
            function onThemeChanged() { map.style = root.mapStyle }
        }
        Connections {
            target: appSettings
            function onMapThemeChanged() { map.style = root.mapStyle }
            function onMapDetailsChanged() {
                if (root.hasController) {
                    root.paneController.applyMapDetails(appSettings.mapDetailVisibility)
                }
            }
        }

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
            // Tick first and unconditionally: geo-anchored objects must re-project on every map
            // movement, including one applied by sync (when the write-back below is suppressed).
            objectsLayer.cameraTick++
            level3Layer.cameraTick++
            if (!root.hasController || root.applyingSync) {
                return
            }
            root.paneController.setCenter(map.coordinate[0], map.coordinate[1])
        }
        onZoomLevelChanged: {
            objectsLayer.cameraTick++
            level3Layer.cameraTick++
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
                root.paneController.applyMapDetails(appSettings.mapDetailVisibility)
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

    // Meteorological overlays sit above radar and below user analysis (§4.3's locked stack).
    WeatherOverlaysLayer {
        anchors.fill: parent
        paneController: root.paneController
        manager: typeof overlayManager !== "undefined" ? overlayManager : null
        cameraTick: objectsLayer.cameraTick
        visible: root.hasController
    }

    Level3ProductLayer {
        id: level3Layer
        anchors.fill: parent
        paneController: root.paneController
        z: 3
    }

    // The User Analysis Layer (§4.3), above the map and its radar rendering but below the pane
    // chrome, so objects never obscure the controls.
    MapObjectsLayer {
        id: objectsLayer
        anchors.fill: parent
        paneController: root.paneController
        objectStore: typeof mapObjectStore !== "undefined" ? mapObjectStore : null
        visible: root.hasController
    }

    // The in-progress measurement (docs/ROADMAP.md §4.4). Drawn separately from MapObjectsLayer
    // because it is tier-1 Temporary state that never enters the store - it belongs to the tool,
    // not to the map.
    MeasurementLayer {
        id: measurementLayer
        anchors.fill: parent
        paneController: root.paneController
        controller: typeof measurementTool !== "undefined" ? measurementTool : null
        cameraTick: objectsLayer.cameraTick
        visible: root.hasController
    }

    // Tier-1 drawing preview. Geographic vertices live in ObjectToolController; this item only
    // projects and presents them, exactly as the committed MapObjectsLayer does.
    Shape {
        anchors.fill: parent
        visible: objectTools.activeTool === 3 && objectTools.drawingActive
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: themeManager.warning
            strokeWidth: 2
            fillColor: "transparent"
            PathPolyline {
                path: {
                    objectsLayer.cameraTick
                    var points = []
                    for (var i = 0; i < objectTools.drawingLatitudes.length; ++i) {
                        points.push(root.paneController.pixelForCoordinate(
                            objectTools.drawingLatitudes[i], objectTools.drawingLongitudes[i]))
                    }
                    return points
                }
            }
        }
    }

    // MeasurementController::Mode::Path. Path is the one mode that cannot be a drag: it has no
    // fixed number of vertices, so it stays click-to-add with right-click to finish.
    readonly property int measurementModePath: 2
    property var snapHighlight: ({ "snapped": false })

    function measurementPlacement(x, y, modifiers) {
        const snap = snapTargets.resolve(root.paneController, x, y,
                                         appSettings.snapTolerancePixels,
                                         (modifiers & Qt.AltModifier) !== 0)
        root.snapHighlight = snap
        if (snap.snapped) {
            return snap
        }
        const geo = root.paneController.coordinateForPixel(x, y)
        return geo.length === 2
            ? { "snapped": false, "latitude": geo[0], "longitude": geo[1],
                "kind": "", "label": "" }
            : null
    }

    // Press-drag-release measurement state. Drag is the primary gesture - every comparable tool
    // measures that way, and click-then-click-again is the thing users coming from them trip
    // over. Click-then-click still works: a press that does not move leaves the origin placed and
    // waits for a second click, so neither habit is punished.
    property bool  measureDragActive: false
    property bool  measureOriginPlaced: false
    property point measurePressPixel: Qt.point(0, 0)

    // Below this, a press-release is a click, not a drag. Small enough not to swallow a genuine
    // short measurement, large enough to absorb the hand tremor in an ordinary click.
    readonly property int measureDragThreshold: 6

    // settings::AppSettings::MeasurementGesture - 0 Both, 1 DragOnly, 2 ClickOnly (§4.4, slice
    // 17). Both stays the default; the preference exists because with both live, a click that
    // does not move leaves a measurement half-started, so a stray click arms something the user
    // did not intend.
    readonly property int measurementGesture:
        (typeof appSettings !== "undefined" && appSettings !== null)
            ? appSettings.measurementGesture : 0

    readonly property int gestureBoth: 0
    readonly property int gestureDragOnly: 1
    readonly property int gestureClickOnly: 2

    // Object placement and measurement. Only active while a tool is selected in the side rail, so
    // ordinary panning is never intercepted - the map's own handlers keep the gesture otherwise.
    MouseArea {
        anchors.fill: parent
        enabled: root.hasController && (root.placementActive || root.measuringActive)
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: root.measuringActive

        // **Required, not a tuning knob.** Taking the press is not enough to own the gesture: the
        // map's DragHandler sits behind this item with default grabPermissions, which include
        // CanTakeOverFromItems, so the moment the pointer crosses the drag threshold it steals the
        // exclusive grab. The result was a measurement that dropped its origin and then panned the
        // map instead of stretching - and because a stolen grab delivers onCanceled rather than
        // onReleased, the release half never ran and the measurement sat unfinished until the user
        // clicked again. preventStealing sets keepMouseGrab, which is what a handler checks before
        // taking over.
        //
        // This does not cost the user panning while a tool is armed: the Shift escape hatch in
        // onPressed declines the press outright, so the map gets the gesture from the start rather
        // than halfway through it.
        preventStealing: true

        onPressed: (mouse) => {
            if (objectTools.activeTool === 3) {
                if (mouse.button === Qt.RightButton) {
                    objectTools.cancelDrawing()
                    return
                }
                if (mouse.modifiers & Qt.ShiftModifier) {
                    mouse.accepted = false
                    return
                }
                const geo = root.paneController.coordinateForPixel(mouse.x, mouse.y)
                if (geo.length === 2) objectTools.beginDrawing(geo[0], geo[1])
                return
            }
            if (!root.measuringActive || mouse.button !== Qt.LeftButton) {
                return
            }

            // Shift hands the gesture back to the map so it pans. Without it, measuring something
            // larger than the viewport would mean disarming the tool, scrolling, and re-arming -
            // and losing the measurement in progress each time.
            if (mouse.modifiers & Qt.ShiftModifier) {
                mouse.accepted = false
                return
            }

            if (measurementTool.mode === root.measurementModePath) {
                return
            }

            root.measurePressPixel = Qt.point(mouse.x, mouse.y)
            root.measureDragActive = true

            // A press while something is already in progress is the second click of a
            // click-then-click, not the start of a new measurement - don't discard the origin.
            root.measureOriginPlaced = measurementTool.active
            if (root.measureOriginPlaced) {
                return
            }

            const point = root.measurementPlacement(mouse.x, mouse.y, mouse.modifiers)
            if (point !== null) {
                measurementTool.beginDrag(point.latitude, point.longitude, root.paneController,
                                          point.kind, point.label)
            }
        }

        onReleased: (mouse) => {
            if (objectTools.activeTool === 3 && mouse.button === Qt.LeftButton) {
                const geo = root.paneController.coordinateForPixel(mouse.x, mouse.y)
                if (geo.length === 2) objectTools.appendDrawingPoint(geo[0], geo[1])
                objectTools.commitDrawing(root.paneController)
                return
            }
            if (!root.measureDragActive || mouse.button !== Qt.LeftButton) {
                return
            }
            root.measureDragActive = false

            const dx = mouse.x - root.measurePressPixel.x
            const dy = mouse.y - root.measurePressPixel.y
            const moved = Math.sqrt(dx * dx + dy * dy) >= root.measureDragThreshold

            // What finishes a measurement depends on the gesture preference (§4.4):
            //   ClickOnly - a drag counts only as the first click; a second press finishes it, so
            //               releasing after dragging must not commit.
            //   DragOnly  - a press that never moved is not a measurement at all. Discard it
            //               rather than leaving an origin on the map waiting for a click the user
            //               has told us they do not intend to make.
            //   Both      - either one finishes it (the shipped default).
            var finishes = false
            if (root.measurementGesture === root.gestureClickOnly) {
                finishes = root.measureOriginPlaced
            } else if (root.measurementGesture === root.gestureDragOnly) {
                if (!moved) {
                    measurementTool.cancel()
                    root.snapHighlight = ({ "snapped": false })
                    return
                }
                finishes = true
            } else {
                finishes = moved || root.measureOriginPlaced
            }

            if (!finishes) {
                return
            }

            const point = root.measurementPlacement(mouse.x, mouse.y, mouse.modifiers)
            if (point !== null) {
                measurementTool.addPoint(point.latitude, point.longitude, root.paneController,
                                         point.kind, point.label)
            }
            measurementTool.commit(root.paneController, objectTools.scopeKind)
            measurementTool.cancel()
            root.snapHighlight = ({ "snapped": false })
        }

        onClicked: (mouse) => {
            if (objectTools.activeTool === 3) {
                return
            }
            const point = root.measurementPlacement(mouse.x, mouse.y, mouse.modifiers)
            if (point === null) {
                return
            }

            if (root.measuringActive) {
                if (mouse.button === Qt.RightButton) {
                    // Right-click ends a measurement: pins it if it has enough vertices,
                    // otherwise just clears the attempt.
                    if (measurementTool.active) {
                        measurementTool.commit(root.paneController, objectTools.scopeKind)
                    }
                    measurementTool.cancel()
                    root.snapHighlight = ({ "snapped": false })
                    return
                }

                // Only Path still adds vertices on click; the fixed-length modes are driven
                // entirely from onPressed/onReleased above, and would otherwise add the far end
                // twice.
                if (measurementTool.mode === root.measurementModePath) {
                    measurementTool.addPoint(point.latitude, point.longitude, root.paneController,
                                             point.kind, point.label)
                }
                return
            }

            objectTools.placeAt(point.latitude, point.longitude, root.paneController)
        }

        // A grab can still be lost legitimately (a Shift hand-off, the window losing focus mid
        // -gesture). onReleased never arrives in that case, so without this measureDragActive
        // would stay true and the next press would be misread as the second click of a
        // click-then-click, silently reusing a stale origin.
        onCanceled: {
            root.measureDragActive = false
            root.measureOriginPlaced = false
            root.snapHighlight = ({ "snapped": false })
        }

        // Live rubber-band while measuring - drives both the drag and the hover half of
        // click-then-click.
        onPositionChanged: (mouse) => {
            if (objectTools.activeTool === 3 && objectTools.drawingActive) {
                const geo = root.paneController.coordinateForPixel(mouse.x, mouse.y)
                if (geo.length === 2) objectTools.appendDrawingPoint(geo[0], geo[1])
                return
            }
            if (!root.measuringActive || !measurementTool.active) {
                return
            }
            const point = root.measurementPlacement(mouse.x, mouse.y, mouse.modifiers)
            if (point !== null) {
                measurementTool.updateCursor(point.latitude, point.longitude,
                                             point.kind, point.label)
            }
        }
    }

    // Visible pre-commit feedback: the endpoint jumps in MeasurementLayer and this halo names
    // the magnetic target. A coordinate rewrite with no cue feels like pointer inaccuracy.
    Rectangle {
        // A cached pixel position is meaningful only while temporary measurement geometry is
        // active. Once commit clears the controller, leaving this visible makes it look attached
        // to the screen while the geographic measurement moves with a subsequent map pan.
        visible: root.measuringActive && measurementTool.active &&
                 root.snapHighlight.snapped === true
        x: visible ? root.snapHighlight.pixelX - width / 2 : 0
        y: visible ? root.snapHighlight.pixelY - height / 2 : 0
        width: 18
        height: 18
        radius: 9
        color: "transparent"
        border.color: themeManager.measurementAccent
        border.width: 2
        z: 5

        Text {
            anchors.left: parent.right
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            text: root.snapHighlight.label || ""
            color: themeManager.measurementAccent
            font.pixelSize: 10
            style: Text.Outline
            styleColor: "#c0000000"
        }
    }

    // Right-click actions on the pane. The immediate need is deletion - before this a committed
    // measurement or marker had no way off the map at all - but this is also where §4.5's pane
    // actions (link, unlink, match location) belong, so it is a menu rather than a bare
    // delete-on-right-click.
    //
    // Hand-rolled from plain QtQuick because the app does not depend on QtQuick.Controls yet;
    // slice 9's shared Controls/ style is the right place to restyle it, not to reinvent it.
    MouseArea {
        anchors.fill: parent
        // A tool being armed means right-click already means something else there (ending a
        // measurement), so the menu stays out of the way until the tools are disarmed.
        enabled: root.hasController && !root.placementActive && !root.measuringActive
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => contextMenu.openAt(mouse.x, mouse.y)
    }

    // Click-away dismissal. Enabled only while the menu is open, so it never intercepts a gesture
    // the rest of the time.
    MouseArea {
        anchors.fill: parent
        enabled: contextMenu.visible
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 9
        onClicked: contextMenu.close()
    }

    Rectangle {
        id: contextMenu
        visible: false
        z: 10
        width: menuColumn.width + 20
        height: menuColumn.height + 12
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        border.width: 1

        // What the menu was opened over, captured at open time rather than re-tested per action:
        // the actions must apply to the object that was highlighted when the user aimed at it,
        // even if a synced pane moves the map underneath in the meantime.
        property int    targetObjectId: -1
        property string targetName: ""
        property int    visibleObjectCount: 0

        readonly property var store: objectsLayer.objectStore

        // wxlens::objects::MapObjectType. Named for the menu so the entry reads "Delete
        // measurement", not "Delete object" - useful precisely when objects overlap.
        function nameForType(type) {
            switch (type) {
            case 0: return "marker"
            case 1: return "line"
            case 2: return "polygon"
            case 3: return "range ring"
            case 4: return "label"
            case 5: return "measurement"
            }
            return "object"
        }

        function openAt(x, y) {
            if (!root.hasController || !contextMenu.store) {
                return
            }

            const objects = contextMenu.store.objectsForPane(root.paneController)
            contextMenu.visibleObjectCount = objects.length
            contextMenu.targetObjectId =
                contextMenu.store.objectAtPixel(root.paneController, x, y, 12)

            contextMenu.targetName = ""
            for (var i = 0; i < objects.length; ++i) {
                if (objects[i].objectId === contextMenu.targetObjectId) {
                    contextMenu.targetName = contextMenu.nameForType(objects[i].objectType)
                    break
                }
            }

            // Nothing to act on - don't flash an empty menu at the user.
            if (contextMenu.targetObjectId < 0 && contextMenu.visibleObjectCount === 0 &&
                !root.showLabel) {
                return
            }

            objectsLayer.highlightedObjectId = contextMenu.targetObjectId

            // Kept inside the pane, so a right-click near an edge doesn't open a menu half
            // outside it.
            contextMenu.x = Math.max(0, Math.min(x, root.width - contextMenu.width - 4))
            contextMenu.y = Math.max(0, Math.min(y, root.height - contextMenu.height - 4))
            contextMenu.visible = true
        }

        function close() {
            contextMenu.visible = false
            contextMenu.targetObjectId = -1
            objectsLayer.highlightedObjectId = -1
        }

        readonly property var entries: {
            var items = []
            if (contextMenu.targetObjectId >= 0) {
                items.push({ label: "Delete " + contextMenu.targetName, action: "delete" })
            }
            if (contextMenu.visibleObjectCount > 0) {
                items.push({ label: contextMenu.visibleObjectCount === 1
                                ? "Clear 1 object from this pane"
                                : "Clear " + contextMenu.visibleObjectCount +
                                  " objects from this pane",
                             action: "clearPane" })
            }
            if (root.showLabel && root.hasController &&
                root.paneController.paneId !== paneGridModel.firstPaneId) {
                items.push({ label: "Match pane 1 view (one time)", action: "matchFirst" })
            }
            return items
        }

        Column {
            id: menuColumn
            anchors.centerIn: parent
            spacing: 1

            Repeater {
                model: contextMenu.entries

                delegate: Rectangle {
                    required property var modelData

                    width: Math.max(entryText.implicitWidth + 16, 170)
                    height: 26
                    radius: themeManager.cornerRadius
                    color: entryArea.containsMouse ? themeManager.controlHover : "transparent"

                    Text {
                        id: entryText
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: parent.modelData.label
                        color: themeManager.textPrimary
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: entryArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (parent.modelData.action === "delete") {
                                contextMenu.store.removeObject(contextMenu.targetObjectId)
                            } else if (parent.modelData.action === "clearPane") {
                                contextMenu.store.removeObjectsInPane(root.paneController)
                            } else if (parent.modelData.action === "matchFirst") {
                                paneGridModel.copyCamera(
                                    paneGridModel.firstPaneId, root.paneController.paneId)
                            }
                            contextMenu.close()
                        }
                    }
                }
            }
        }
    }

    // Bumped whenever this pane's source publishes new data, so the beam-geometry readout below
    // re-runs its probe. probeSourceAt is a method call, which a binding cannot know has gone
    // stale - the same reason objects re-project off cameraTick.
    property int sourceTick: 0

    Connections {
        target: root.hasController ? root.paneController : null
        function onSourceDataChanged() { root.sourceTick++ }
    }

    // The far end of the in-progress measurement - what §4.7's geometry readout interrogates.
    // Reading `measurementTool.points` (a notifying property) is what makes this live as the
    // endpoint is dragged, without the geometry panel having to know about measurement at all.
    readonly property var measureTargetPoint: {
        if (!root.measuringActive || !root.hasController ||
            measurementTool.activePaneId !== root.paneController.paneId) {
            return null
        }
        const flat = measurementTool.points
        if (flat.length < 2) {
            return null
        }
        return { latitude: flat[flat.length - 2], longitude: flat[flat.length - 1] }
    }

    // Measurement readout. Progressive disclosure per §4.4/§5.3: the one-line result is always
    // visible while measuring; per-segment detail and the §4.7 radar-geometry breakdown are
    // available but not forced on the user.
    Rectangle {
        visible: root.measuringActive && measurementTool.readout !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 22
        width: readoutColumn.width + 20
        height: readoutColumn.height + 12
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        border.width: 1

        Column {
            id: readoutColumn
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: root.measuringActive ? measurementTool.readout : ""
                color: themeManager.textPrimary
                font.pixelSize: 12
            }

            Text {
                visible: root.measuringActive && measurementTool.segments.length > 1
                text: root.measuringActive && measurementTool.segments.length > 0
                    ? "last leg " + measurementTool.formatDistance(
                        measurementTool.segments[measurementTool.segments.length - 1].distanceMeters)
                    : ""
                color: themeManager.textMuted
                font.pixelSize: 10
            }

            // Radar geometry (§4.7), extending the measurement's far end into a full beam
            // interrogation. Collapsed by default; it hides itself entirely on a pane with no
            // radar source, so a future satellite pane does not grow an empty section.
            RadarGeometryPanel {
                id: radarGeometryPanel
                paneController: root.paneController
                sourceTick: root.sourceTick
                onConfigureRequested: (sectionId) => root.configureRequested(sectionId)
                targetLatitude: root.measureTargetPoint
                    ? root.measureTargetPoint.latitude : NaN
                targetLongitude: root.measureTargetPoint
                    ? root.measureTargetPoint.longitude : NaN
            }

            Text {
                text: "click to add · right-click to finish"
                color: themeManager.textMuted
                font.pixelSize: 9
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
            radius: themeManager.cornerRadius
            color: linked ? themeManager.controlActive : themeManager.control
            border.color: linked ? themeManager.primary : themeManager.border
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
                color: parent.linked ? themeManager.textPrimary : themeManager.textMuted
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
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 6
        visible: root.showLabel
        width: productLabel.implicitWidth + 14
        height: 24
        radius: themeManager.cornerRadius
        color: themeManager.control
        border.color: themeManager.border
        Text {
            id: productLabel
            anchors.centerIn: parent
            text: root.hasController
                ? root.paneController.sourceKey + " · " + root.paneController.productName + " ▾"
                : ""
            font.pixelSize: 11
            color: themeManager.textSecondary
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.productBrowserOpen = !root.productBrowserOpen
        }
    }

    ProductBrowser {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 6
        z: 20
        visible: root.productBrowserOpen && root.hasController
        paneController: root.paneController
        onCloseRequested: root.productBrowserOpen = false
    }

    Rectangle {
        visible: root.hasController && root.paneController.productDetailsText !== ""
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        width: Math.min(420, parent.width - 16)
        height: Math.min(180, detailsText.implicitHeight + 20)
        z: 8
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        clip: true
        Text {
            id: detailsText
            anchors.fill: parent
            anchors.margins: 10
            text: root.hasController ? root.paneController.productDetailsText : ""
            color: themeManager.textPrimary
            font.pixelSize: 11
            wrapMode: Text.Wrap
            elide: Text.ElideRight
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
        color: themeManager.textSecondary
        style: Text.Outline
        styleColor: "#90000000"
    }
}

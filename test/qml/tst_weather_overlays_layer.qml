// SPDX-License-Identifier: MIT
//
// WeatherOverlaysLayer after the move to GPU rendering (ROADMAP: "Fix camera-movement cost in
// warning/placefile overlays").
//
// Warning polygons and placefile vector items are now built and filtered in C++
// (render::BuildOverlayGeometry, render::ResolveNearbyWarningsOnly - covered by
// overlay_geometry.test.cpp, including the nearby-filter cases this file used to hold). What is
// left in QML, and tested here, is the layer's glue: handing the manager to the pane's
// PaneController, and selecting the icon and text items it still draws as QML items.
import QtQuick
import QtTest
import WxLens.App

TestCase {
    id: testCase
    name: "WeatherOverlaysLayer"

    // Stands in for wxlens::panes::PaneController. Only the members the layer actually touches are
    // present; a wider fake would just be more surface to drift from the real class.
    component FakePane: QtObject {
        property var overlayManager: null
        function pixelForCoordinate(lat, lon) { return Qt.point(lon * 10, lat * 10) }
    }

    // Stands in for wxlens::overlays::OverlayManager.
    component FakeManager: QtObject {
        property bool warningsVisible: true
        property bool placefilesVisible: true
        property bool nearbyWarningsOnly: false
        property real nearbyWarningsRangeMeters: 230000.0
        property var warningPolygons: []
        property var placefileItems: []
        signal warningsChanged()
        signal placefilesChanged()
    }

    Component {
        id: layerComponent
        WeatherOverlaysLayer {}
    }

    Component {
        id: paneComponent
        FakePane {}
    }

    property FakeManager manager: FakeManager {}

    readonly property var mixedItems: [
        { kind: "line", coordinates: [35, -97, 36, -96], color: "#ff0000", width: 2 },
        { kind: "polygon", contours: [[35, -97, 35, -96, 36, -96]], color: "#00ff00" },
        { kind: "triangles", coordinates: [35, -97, 35, -96, 36, -96], color: "#0000ff" },
        { kind: "text", latitude: 35, longitude: -97, text: "OKC", color: "#ffffff" },
        { kind: "text", latitude: 36, longitude: -96, text: "TUL", color: "#ffffff" },
        { kind: "icon", latitude: 35.5, longitude: -96.5, source: "", width: 16, height: 16,
          hotX: 8, hotY: 8, iconNumber: 0, angle: 0 }
    ]

    function init() {
        // Reset between cases - the manager fake is shared across the whole TestCase.
        manager.placefilesVisible = true
        manager.placefileItems = []
    }

    function makeLayer(pane) {
        const layer = createTemporaryObject(layerComponent, testCase,
                                            { paneController: pane, manager: manager })
        verify(layer !== null, "layer instantiated")
        return layer
    }

    function test_hands_the_manager_to_the_pane_controller() {
        const pane = createTemporaryObject(paneComponent, testCase)
        makeLayer(pane)
        compare(pane.overlayManager, manager,
                "PaneController.overlayManager is what feeds the C++ overlay layer")
    }

    function test_only_text_and_icon_items_stay_in_qml() {
        manager.placefileItems = mixedItems
        const layer = makeLayer(createTemporaryObject(paneComponent, testCase))
        compare(layer.textItems.length, 2, "both text items")
        compare(layer.iconItems.length, 1, "the icon item")
        compare(layer.textItems[0].text, "OKC")
        compare(layer.textItems[1].text, "TUL")
    }

    function test_hidden_placefiles_draw_no_qml_items() {
        manager.placefileItems = mixedItems
        manager.placefilesVisible = false
        const layer = makeLayer(createTemporaryObject(paneComponent, testCase))
        compare(layer.textItems.length, 0)
        compare(layer.iconItems.length, 0)
    }

    function test_items_follow_a_data_change() {
        const layer = makeLayer(createTemporaryObject(paneComponent, testCase))
        compare(layer.textItems.length, 0)
        manager.placefileItems = mixedItems
        compare(layer.textItems.length, 2, "re-filtered when placefileItems changes")
    }

    function test_anchor_uses_the_pane_projection() {
        const layer = makeLayer(createTemporaryObject(paneComponent, testCase))
        const anchor = layer.anchorFor({ latitude: 35, longitude: -97 })
        compare(anchor.x, -970)
        compare(anchor.y, 350)
    }

    // The layer must tolerate a null controller: PaneGrid destroys PaneControllers while their
    // delegates are still tearing down, which is the same hazard PaneHost's hasController guard
    // exists for.
    function test_null_controller_is_not_a_crash() {
        manager.placefileItems = mixedItems
        const layer = makeLayer(null)
        const anchor = layer.anchorFor({ latitude: 35, longitude: -97 })
        compare(anchor.x, 0)
        compare(anchor.y, 0)
        compare(layer.textItems.length, 2, "item selection does not depend on the controller")
    }
}

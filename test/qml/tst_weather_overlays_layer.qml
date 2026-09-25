// SPDX-License-Identifier: MIT
//
// WeatherOverlaysLayer's nearby-warnings filter (ROADMAP slice 19's first QML coverage).
//
// This predicate decides which warning polygons a pane draws. It is pure QML - the C++ side
// contributes only the persisted default and the range constant - so wxlens-app-test cannot
// reach it, which is why it shipped untested.
import QtQuick
import QtTest
import WxLens.App

TestCase {
    id: testCase
    name: "WeatherOverlaysLayer"

    // KEAX, the site the application opens on, so the numbers below are realistic rather than
    // arbitrary. One degree of latitude is ~111 km, comfortably inside and outside the 230 km
    // cutoff at 1 and 3 degrees respectively.
    readonly property real siteLatitude: 38.810
    readonly property real siteLongitude: -94.264

    // Stands in for wxlens::panes::PaneController. Only the members the layer actually reads are
    // present; a wider fake would just be more surface to drift from the real class.
    // The signals matter as much as the properties: the layer's Connections blocks bind to them
    // by name, and a fake missing one produces a "no signal of the target matches the name"
    // warning per instantiation rather than a failure. Keeping them here is what makes that
    // warning mean a real mismatch with the C++ class instead of a gap in this file.
    component FakePane: QtObject {
        property real homeLatitude: testCase.siteLatitude
        property real homeLongitude: testCase.siteLongitude
        property int warningsFilterOverride: 0
        signal productChanged()
        function pixelForCoordinate(lat, lon) { return Qt.point(0, 0) }
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

    property FakePane pane: FakePane {}
    property FakeManager manager: FakeManager {}

    function makeLayer() {
        const layer = createTemporaryObject(layerComponent, testCase,
                                            { paneController: pane, manager: manager })
        verify(layer !== null, "layer instantiated")
        return layer
    }

    function warningAt(lat, lon) {
        return { latitude: lat, longitude: lon, coordinates: [], color: "#ff0000", label: "TOR" }
    }

    function init() {
        // Reset between cases - the fakes are shared across the whole TestCase.
        pane.warningsFilterOverride = 0
        manager.nearbyWarningsOnly = false
    }

    function test_distanceMeters_matches_known_separation() {
        const layer = makeLayer()
        // One degree of latitude is ~111.2 km anywhere on the globe. A 1% tolerance is well
        // inside what distinguishes "near" from "far" at a 230 km cutoff.
        const oneDegree = layer.distanceMeters(siteLatitude, siteLongitude,
                                               siteLatitude + 1.0, siteLongitude)
        verify(Math.abs(oneDegree - 111195) < 1112,
               "one degree of latitude measured " + oneDegree + " m")
        compare(layer.distanceMeters(siteLatitude, siteLongitude, siteLatitude, siteLongitude), 0,
                "zero separation")
    }

    function test_filter_off_keeps_every_warning() {
        const layer = makeLayer()
        verify(layer.isNearby(warningAt(siteLatitude, siteLongitude)), "co-located warning kept")
        verify(layer.isNearby(warningAt(siteLatitude + 30, siteLongitude + 30)),
               "a warning on the other side of the country is kept when filtering is off")
    }

    function test_global_default_filters_distant_warnings() {
        manager.nearbyWarningsOnly = true
        const layer = makeLayer()
        verify(layer.isNearby(warningAt(siteLatitude + 1, siteLongitude)),
               "~111 km is inside the 230 km range")
        verify(!layer.isNearby(warningAt(siteLatitude + 3, siteLongitude)),
               "~333 km is outside the 230 km range")
    }

    function test_pane_override_forces_all_against_global_default() {
        manager.nearbyWarningsOnly = true
        pane.warningsFilterOverride = 1
        const layer = makeLayer()
        verify(layer.isNearby(warningAt(siteLatitude + 3, siteLongitude)),
               "override 1 shows a distant warning the global default would hide")
    }

    function test_pane_override_forces_nearby_against_global_default() {
        manager.nearbyWarningsOnly = false
        pane.warningsFilterOverride = 2
        const layer = makeLayer()
        verify(!layer.isNearby(warningAt(siteLatitude + 3, siteLongitude)),
               "override 2 hides a distant warning the global default would show")
        verify(layer.isNearby(warningAt(siteLatitude + 1, siteLongitude)),
               "override 2 still shows a nearby warning")
    }

    function test_override_follows_global_default_when_unset() {
        pane.warningsFilterOverride = 0
        const layer = makeLayer()
        verify(layer.isNearby(warningAt(siteLatitude + 3, siteLongitude)), "follows default off")
        manager.nearbyWarningsOnly = true
        verify(!layer.isNearby(warningAt(siteLatitude + 3, siteLongitude)),
               "follows the default when it flips, without the pane being touched")
    }

    // The layer must tolerate a null controller: PaneGrid destroys PaneControllers while their
    // delegates are still tearing down, which is the same hazard PaneHost's hasController guard
    // exists for.
    function test_null_controller_is_not_a_crash() {
        const layer = createTemporaryObject(layerComponent, testCase,
                                            { paneController: null, manager: manager })
        verify(layer !== null, "layer instantiated with no controller")
        verify(layer.isNearby(warningAt(siteLatitude, siteLongitude)),
               "filtering is inert without a controller rather than throwing")
    }
}

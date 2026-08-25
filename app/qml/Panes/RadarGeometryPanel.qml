// SPDX-License-Identifier: MIT
import QtQuick

// The radar-geometry interrogation readout (docs/ROADMAP.md §4.7): "at this distance from the
// radar, how high is the radar actually sampling?"
//
// Progressive disclosure per §4.7/§5.3 - collapsed to a single header by default, because the
// common measurement is already answered by the range/bearing line above it. Expanding is what
// asks the fuller question, and the choice sticks for the session.
//
// This file holds no geometry and no row definitions. Values and their "unavailable" wording come
// from PaneController::probeSourceAt; the row list, its labels, which probe key each row shows and
// whether a row is currently visible all come from settings::AppSettings::geometryRows. One
// catalogue owns the readout's shape, so the settings checklist and this readout cannot disagree
// about what exists.
Column {
    id: root

    // The nimbus::panes::PaneController to interrogate.
    required property var paneController

    // The point being interrogated - the far end of the in-progress measurement.
    property real targetLatitude: NaN
    property real targetLongitude: NaN

    // Bumped when the pane's source publishes new data, so a readout left open while a sweep
    // finishes loading picks up the real elevation angle instead of sitting on "waiting for sweep
    // data" until the cursor next moves. probeSourceAt is a method call, which a binding cannot
    // know has gone stale - the same reason MapObjectsLayer reads a cameraTick.
    property int sourceTick: 0

    property bool expanded: false

    // Emitted when the user asks to change what this readout shows (§4.5: every quick control
    // links to the setting that governs its default). PaneHost routes it to the settings surface.
    signal configureRequested(string sectionId)

    readonly property bool hasTarget:
        paneController !== null && paneController !== undefined &&
        !isNaN(targetLatitude) && !isNaN(targetLongitude)

    readonly property var probe: {
        if (!root.hasTarget) {
            return null
        }
        root.sourceTick
        // probeSourceAt returns preformatted strings, so changing units must invalidate this
        // method-call binding even though the coordinate and source did not change.
        if (typeof appSettings !== "undefined" && appSettings !== null) {
            appSettings.distanceUnits
        }
        return root.paneController.probeSourceAt(root.targetLatitude, root.targetLongitude)
    }

    // Only offered where it means something. A pane showing a non-radar source (Phase 2/3) gets
    // its own probe shape, not this one with empty rows.
    readonly property bool available: root.probe !== null && root.probe.available === true

    readonly property var rows: {
        if (!root.available || typeof appSettings === "undefined" || appSettings === null) {
            return []
        }

        const p = root.probe
        const pending = !p.elevationAngleKnown
        var out = []

        const catalogue = appSettings.geometryRows
        for (var i = 0; i < catalogue.length; ++i) {
            const row = catalogue[i]
            if (!row.visible) {
                continue
            }
            out.push({
                label: row.label,
                value: p[row.valueKey],
                // "always" marks a row whose value is a statement about what is not known;
                // "whenElevationUnknown" marks one that is only meaningless before a sweep loads.
                dim: row.dimWhen === "always" ||
                     (row.dimWhen === "whenElevationUnknown" && pending)
            })
        }
        return out
    }

    visible: root.available
    spacing: 3

    Row {
        spacing: 6

        Item {
            width: headerText.implicitWidth
            height: headerText.implicitHeight + 4

            Text {
                id: headerText
                anchors.verticalCenter: parent.verticalCenter
                text: (root.expanded ? "▾" : "▸") + "  Radar geometry"
                color: themeManager.textMuted
                font.pixelSize: 10
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        // §4.5's other direction: the moment a user finds themselves wanting different rows, the
        // control that shows them should be able to open the setting that governs it - not the top
        // of Settings, the specific section.
        Item {
            visible: root.expanded
            width: configText.implicitWidth + 6
            height: headerText.implicitHeight + 4

            Text {
                id: configText
                anchors.centerIn: parent
                text: "⚙"
                color: configArea.containsMouse ? themeManager.textPrimary : themeManager.textMuted
                font.pixelSize: 11
            }

            MouseArea {
                id: configArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.configureRequested("radar-geometry")
            }
        }
    }

    Column {
        visible: root.expanded
        spacing: 2

        Repeater {
            model: root.rows

            delegate: Row {
                required property var modelData

                spacing: 8

                Text {
                    width: 96
                    text: parent.modelData.label
                    color: themeManager.textMuted
                    font.pixelSize: 10
                }

                Text {
                    text: parent.modelData.value
                    // Dimmed where the value is a statement about what is *not* known, so a
                    // reader can tell a figure from an apology at a glance.
                    color: parent.modelData.dim ? themeManager.textMuted : themeManager.textPrimary
                    font.pixelSize: 10
                    font.italic: parent.modelData.dim
                }
            }
        }

        // Hiding every row would otherwise collapse the section to nothing, which reads as a bug
        // rather than as a choice the user made.
        Text {
            visible: root.rows.length === 0
            text: "all rows hidden — ⚙ to change"
            color: themeManager.textMuted
            font.pixelSize: 10
            font.italic: true
        }
    }
}

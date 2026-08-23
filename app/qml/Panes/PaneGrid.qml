// SPDX-License-Identifier: MIT
import QtQuick

// The pane grid (docs/ROADMAP.md §4.6), driven by nimbus::panes::PaneGridModel. A 1x1 grid is
// just the degenerate case of the same layout, not a separate code path.
//
// Cross-pane synchronization (§4.1-4.2) is slice 5 - every pane here is fully independent.
Item {
    id: root

    // The nimbus::panes::PaneGridModel backing this grid.
    required property var model

    // `model` is a context property that QML tears down before this item during application exit,
    // so these must tolerate it going null rather than throwing a burst of TypeErrors on close.
    readonly property bool hasModel: root.model !== null && root.model !== undefined

    readonly property int columns: root.hasModel ? Math.max(1, root.model.gridWidth) : 1
    readonly property int rows: root.hasModel ? Math.max(1, root.model.gridHeight) : 1
    readonly property int gutter: 2

    Repeater {
        model: root.model

        delegate: PaneHost {
            required property var pane
            required property int index

            paneController: pane
            showLabel: root.columns * root.rows > 1

            // Explicit geometry rather than a GridLayout: each cell's size is a pure function of
            // the grid dimensions, and the trailing row/column absorbs the rounding remainder so
            // the panes always exactly fill the available area with no seam or overflow.
            readonly property int column: index % root.columns
            readonly property int row: Math.floor(index / root.columns)

            readonly property int cellWidth: Math.floor(
                (root.width - root.gutter * (root.columns - 1)) / root.columns)
            readonly property int cellHeight: Math.floor(
                (root.height - root.gutter * (root.rows - 1)) / root.rows)

            x: column * (cellWidth + root.gutter)
            y: row * (cellHeight + root.gutter)

            width: column === root.columns - 1 ? root.width - x : cellWidth
            height: row === root.rows - 1 ? root.height - y : cellHeight
        }
    }
}

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

    readonly property int columns: Math.max(1, root.model.gridWidth)
    readonly property int rows: Math.max(1, root.model.gridHeight)
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

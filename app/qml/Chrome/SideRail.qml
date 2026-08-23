// SPDX-License-Identifier: MIT
import QtQuick

// Successor to scwx-qt/ui/radar_toolbox_rail_widget.cpp. Most tool actions land alongside the
// features they open (measurement in slice 7, palette editor in slice 9, etc.); the grid-size
// buttons are here because slice 4's pane grid needs *some* way to be driven, and the real pane
// chrome/quick controls (§4.5) don't exist until slice 5.
Rectangle {
    id: root
    width: 56
    color: "#14181e"

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: "#2a3138"
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        // Object tools (§4.3). Selecting one arms placement in every pane; clicking a pane then
        // commits a Pinned object there. Selecting the same tool again disarms it, so the map is
        // never left in a state where an ordinary click creates something unexpected.
        Repeater {
            model: [
                { label: "●", tool: 1, hint: "Marker" },
                { label: "◎", tool: 2, hint: "Range ring" }
            ]

            delegate: Rectangle {
                required property var modelData

                width: 32
                height: 32
                radius: 6
                color: active ? "#3a2f1e" : "#20262e"
                border.color: active ? "#b08a4a" : "#2f3742"
                border.width: 1

                readonly property bool active:
                    typeof objectTools !== "undefined" &&
                    objectTools.activeTool === modelData.tool

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData.label
                    color: parent.active ? "#f2dcb0" : "#8d99a8"
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: objectTools.activeTool =
                        parent.active ? 0 : parent.modelData.tool
                }
            }
        }

        // Scope for newly placed objects (§4.3): current pane, the pane's sync group, or all
        // panes. Placed next to the tools because it changes what placing one *means*.
        Rectangle {
            width: 32
            height: 32
            radius: 6
            color: "#20262e"
            border.color: "#2f3742"
            border.width: 1
            visible: typeof objectTools !== "undefined" && objectTools.activeTool !== 0

            // MapObjectScopeKind: 0 CurrentPaneOnly, 1 SyncGroup, 3 AllPanes. SameLocation (2) is
            // omitted here only because it needs no explicit choice to be useful yet.
            readonly property var labels: ["1", "G", "", "A"]

            Text {
                anchors.centerIn: parent
                text: typeof objectTools !== "undefined"
                    ? parent.labels[objectTools.scopeKind] : ""
                color: "#8d99a8"
                font.pixelSize: 13
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                // Cycles CurrentPaneOnly -> SyncGroup -> AllPanes, skipping SameLocation.
                onClicked: {
                    const order = [0, 1, 3]
                    const next = (order.indexOf(objectTools.scopeKind) + 1) % order.length
                    objectTools.scopeKind = order[next]
                }
            }
        }

        Rectangle {
            width: 32
            height: 1
            color: "#2a3138"
        }

        Repeater {
            model: [
                { label: "1", w: 1, h: 1 },
                { label: "2", w: 2, h: 1 },
                { label: "4", w: 2, h: 2 },
                { label: "9", w: 3, h: 3 }
            ]

            delegate: Rectangle {
                required property var modelData

                width: 32
                height: 32
                radius: 6
                color: active ? "#2b3a4d" : "#20262e"
                border.color: active ? "#4a7ab0" : "#2f3742"
                border.width: 1

                // paneGridModel is a context property torn down before this item on application
                // exit, so it has to be null-checked or every close throws TypeErrors here.
                readonly property bool active:
                    paneGridModel !== null && paneGridModel !== undefined &&
                    paneGridModel.gridWidth === modelData.w &&
                    paneGridModel.gridHeight === modelData.h

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData.label
                    color: parent.active ? "#dce6f2" : "#8d99a8"
                    font.pixelSize: 13
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: paneGridModel.setGridSize(parent.modelData.w, parent.modelData.h)
                }
            }
        }
    }
}

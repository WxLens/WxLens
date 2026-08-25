// SPDX-License-Identifier: MIT
import QtQuick

// Successor to scwx-qt/ui/radar_toolbox_rail_widget.cpp. Most tool actions land alongside the
// features they open (measurement in slice 7, palette editor in slice 9, etc.); the grid-size
// buttons are here because slice 4's pane grid needs *some* way to be driven, and the real pane
// chrome/quick controls (§4.5) don't exist until slice 5.
Rectangle {
    id: root
    width: 56
    color: themeManager.surface

    // §4.5's "every quick control links to the setting that governs its default" - the scope
    // control below changes one object once; the moment a user sets it the same way repeatedly,
    // what they actually want is the default, and they should not have to go hunting for it.
    signal configureRequested(string sectionId)

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: themeManager.border
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
                radius: themeManager.cornerRadius
                color: active ? themeManager.controlActive : themeManager.control
                border.color: active ? themeManager.warning : themeManager.border
                border.width: 1

                readonly property bool active:
                    typeof objectTools !== "undefined" &&
                    objectTools.activeTool === modelData.tool

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData.label
                    color: parent.active ? themeManager.textPrimary : themeManager.textMuted
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (parent.active) {
                            objectTools.activeTool = 0
                        } else {
                            // Only one tool family may claim clicks at a time.
                            measurementTool.mode = 0
                            objectTools.activeTool = parent.modelData.tool
                        }
                    }
                }
            }
        }

        // Scope for newly placed objects (§4.3): current pane, the pane's sync group, or all
        // panes. Placed next to the tools because it changes what placing one *means*.
        //
        // Shown for measurements too, not just object placement: a committed measurement is an
        // object like any other, and scope is what decides whether it stays on the pane it was
        // drawn in or appears across all of them. Hiding it during measurement made "keep this on
        // every pane" unreachable for exactly the objects people most want it for.
        Rectangle {
            width: 32
            height: 32
            radius: themeManager.cornerRadius
            color: themeManager.control
            border.color: themeManager.border
            border.width: 1
            visible: (typeof objectTools !== "undefined" && objectTools.activeTool !== 0) ||
                     (typeof measurementTool !== "undefined" && measurementTool.mode !== 0)

            // MapObjectScopeKind: 0 CurrentPaneOnly, 1 SyncGroup, 3 AllPanes. SameLocation (2) is
            // omitted here only because it needs no explicit choice to be useful yet.
            readonly property var labels: ["1", "G", "", "A"]

            Text {
                anchors.centerIn: parent
                text: typeof objectTools !== "undefined"
                    ? parent.labels[objectTools.scopeKind] : ""
                color: themeManager.textMuted
                font.pixelSize: 13
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                // Left cycles CurrentPaneOnly -> SyncGroup -> AllPanes, skipping SameLocation.
                // Right opens the setting that decides which of them new objects start in.
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root.configureRequested("objects")
                        return
                    }
                    const order = [0, 1, 3]
                    const next = (order.indexOf(objectTools.scopeKind) + 1) % order.length
                    objectTools.scopeKind = order[next]
                }
            }
        }

        Rectangle {
            width: 32
            height: 1
            color: themeManager.border
        }

        // Measurement modes (§4.4). Selecting one disarms the object tools, since both act on
        // clicks in a pane.
        Repeater {
            model: [
                { label: "↔", mode: 1, hint: "Point to point" },
                { label: "◄", mode: 2, hint: "Radar to point" },
                { label: "⋯", mode: 3, hint: "Path" }
            ]

            delegate: Rectangle {
                required property var modelData

                width: 32
                height: 32
                radius: themeManager.cornerRadius
                color: active ? themeManager.controlActive : themeManager.control
                border.color: active ? themeManager.measurementAccent : themeManager.border
                border.width: 1

                readonly property bool active:
                    typeof measurementTool !== "undefined" &&
                    measurementTool.mode === modelData.mode

                Text {
                    anchors.centerIn: parent
                    text: parent.modelData.label
                    color: parent.active ? themeManager.textPrimary : themeManager.textMuted
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (parent.active) {
                            measurementTool.mode = 0
                        } else {
                            objectTools.activeTool = 0
                            measurementTool.mode = parent.modelData.mode
                        }
                    }
                }
            }
        }

        Rectangle {
            width: 32
            height: 1
            color: themeManager.border
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
                radius: themeManager.cornerRadius
                color: active ? themeManager.controlActive : themeManager.control
                border.color: active ? themeManager.primary : themeManager.border
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
                    color: parent.active ? themeManager.textPrimary : themeManager.textMuted
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

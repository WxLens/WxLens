// SPDX-License-Identifier: MIT
import QtQuick

// Slice 16's single bottom zone: global tools and layout controls flank the time controls for
// the selected pane. All icons are drawn primitives, so their meaning is not tied to a font's
// symbol coverage and asymmetric layouts remain visually distinct.
Rectangle {
    id: root
    required property var gridModel
    signal configureRequested(string sectionId)
    property bool analysisOpen: false
    property bool layoutOpen: false
    property int lastGridWidth: 2
    property int lastGridHeight: 2

    function toggleLastLayout() {
        if (gridModel.gridWidth === 1 && gridModel.gridHeight === 1) {
            gridModel.setGridSize(lastGridWidth, lastGridHeight)
        } else {
            lastGridWidth = gridModel.gridWidth
            lastGridHeight = gridModel.gridHeight
            gridModel.setGridSize(1, 1)
        }
        layoutOpen = false
    }

    width: Math.min(parent ? parent.width - 24 : controls.implicitWidth + 24,
                    controls.implicitWidth + 24)
    height: 50
    Accessible.role: Accessible.ToolBar
    Accessible.name: "Pane, time, measurement, and layout controls"
    radius: appSettings.controlBarDocked ? 0 : themeManager.cornerRadius * 1.5
    color: themeManager.elevatedSurface
    border.color: themeManager.border
    border.width: 1
    opacity: hover.hovered || appSettings.controlBarDocked ? 0.98 : 0.78
    Behavior on opacity { NumberAnimation { duration: 180 } }

    HoverHandler { id: hover }

    component ToolButton: Rectangle {
        id: button
        property bool active: false
        property color accent: themeManager.primary
        property string hint: ""
        signal triggered()
        signal held()
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: hint
        width: 32; height: 32
        radius: themeManager.cornerRadius
        color: active ? themeManager.controlActive
                      : (area.containsMouse ? themeManager.controlHover : themeManager.control)
        border.color: active ? accent : themeManager.border
        MouseArea {
            id: area
            property bool heldThisPress: false
            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onPressed: heldThisPress = false
            onPressAndHold: {
                heldThisPress = true
                button.held()
            }
            onClicked: {
                if (!heldThisPress)
                    button.triggered()
            }
        }
        Keys.onSpacePressed: button.triggered()
        Keys.onReturnPressed: button.triggered()
    }

    Rectangle {
        visible: root.analysisOpen
        anchors.left: parent.left
        anchors.bottom: parent.top
        anchors.bottomMargin: 8
        width: analysisTools.implicitWidth + 20
        height: 48
        radius: themeManager.cornerRadius * 1.25
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        border.width: 1
        opacity: 0.98

        Row {
            id: analysisTools
            anchors.centerIn: parent
            spacing: 5

            ToolButton {
                active: objectTools.activeTool === 1; accent: themeManager.warning; hint: "Marker"
                onTriggered: {
                    measurementTool.mode = 0
                    objectTools.activeTool = active ? 0 : 1
                }
                Rectangle { anchors.centerIn: parent; width: 9; height: 9; radius: 5; color: themeManager.textPrimary }
            }
            ToolButton {
                active: objectTools.activeTool === 2; accent: themeManager.warning; hint: "Range ring"
                onTriggered: {
                    measurementTool.mode = 0
                    objectTools.activeTool = active ? 0 : 2
                }
                Rectangle { anchors.centerIn: parent; width: 16; height: 16; radius: 8; color: "transparent"; border.width: 2; border.color: themeManager.textPrimary }
                Rectangle { anchors.centerIn: parent; width: 4; height: 4; radius: 2; color: themeManager.textPrimary }
            }
            ToolButton {
                active: objectTools.activeTool === 3; accent: themeManager.warning; hint: "Draw line"
                onTriggered: {
                    measurementTool.mode = 0
                    objectTools.activeTool = active ? 0 : 3
                }
                Rectangle { width: 20; height: 2; color: themeManager.textPrimary; anchors.centerIn: parent; rotation: -32 }
                Rectangle { width: 5; height: 5; radius: 3; color: themeManager.textPrimary; x: 6; y: 22 }
                Rectangle { width: 5; height: 5; radius: 3; color: themeManager.textPrimary; x: 23; y: 7 }
            }
            ToolButton {
                visible: objectTools.activeTool !== 0 || measurementTool.mode !== 0
                hint: "Object scope (right-click for default)"
                Text {
                    anchors.centerIn: parent
                    text: ["Pane", "Group", "", "All"][objectTools.scopeKind]
                    color: themeManager.textSecondary; font.pixelSize: 9; font.bold: true
                }
                MouseArea {
                    anchors.fill: parent; acceptedButtons: Qt.RightButton
                    onClicked: root.configureRequested("objects")
                }
                onTriggered: {
                    const order = [0, 1, 3]
                    objectTools.scopeKind = order[(order.indexOf(objectTools.scopeKind) + 1) % order.length]
                }
            }
            ToolButton {
                hint: "Measurement help and settings"
                onTriggered: root.configureRequested("measurement")
                Text { anchors.centerIn: parent; text: "?"; color: themeManager.textSecondary; font.pixelSize: 14; font.bold: true }
            }
        }
    }

    Rectangle {
        visible: root.layoutOpen
        anchors.right: parent.right
        anchors.bottom: parent.top
        anchors.bottomMargin: 8
        width: layoutTools.implicitWidth + 20
        height: 48
        radius: themeManager.cornerRadius * 1.25
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        border.width: 1
        opacity: 0.98

        Row {
            id: layoutTools
            anchors.centerIn: parent
            spacing: 5

            Repeater {
                model: [{w:1,h:1},{w:2,h:1},{w:1,h:2},{w:2,h:2},{w:3,h:3}]
                delegate: ToolButton {
                    id: layoutChoice
                    required property var modelData
                    active: root.gridModel.gridWidth === modelData.w && root.gridModel.gridHeight === modelData.h
                    onTriggered: {
                        root.lastGridWidth = modelData.w
                        root.lastGridHeight = modelData.h
                        root.gridModel.setGridSize(modelData.w, modelData.h)
                        root.layoutOpen = false
                    }
                    Item {
                        width: 20; height: 20; anchors.centerIn: parent
                        Repeater {
                            model: layoutChoice.modelData.w * layoutChoice.modelData.h
                            Rectangle {
                                required property int index
                                readonly property int cols: layoutChoice.modelData.w
                                readonly property int rows: layoutChoice.modelData.h
                                width: (20 - (cols - 1) * 2) / cols
                                height: (20 - (rows - 1) * 2) / rows
                                x: (index % cols) * (width + 2)
                                y: Math.floor(index / cols) * (height + 2)
                                color: themeManager.textSecondary
                                radius: 1
                            }
                        }
                    }
                }
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        contentWidth: controls.implicitWidth
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width

        Row {
            id: controls
            x: Math.max(0, (parent.width - width) / 2)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

        ToolButton {
            id: analysisButton
            active: root.analysisOpen
            hint: "Analysis tools"
            onTriggered: { root.analysisOpen = !root.analysisOpen; root.layoutOpen = false }
            Text { anchors.centerIn: parent; text: "+"; color: themeManager.textSecondary; font.pixelSize: 18 }
        }

        Rectangle { width: 1; height: 28; anchors.verticalCenter: parent.verticalCenter; color: themeManager.border }

        ToolButton {
            active: measurementTool.mode === appSettings.preferredMeasurementTool
            accent: themeManager.measurementAccent
            hint: appSettings.preferredMeasurementTool === 1 ? "Measure point to point" : "Measure path"
            onTriggered: {
                objectTools.activeTool = 0
                measurementTool.mode = active ? 0 : appSettings.preferredMeasurementTool
            }
            Rectangle { width: 20; height: 2; color: themeManager.textPrimary; anchors.centerIn: parent; rotation: -32 }
            Rectangle { width: 2; height: 7; color: themeManager.textPrimary; x: 8; y: 20; rotation: -32 }
            Rectangle { width: 2; height: 7; color: themeManager.textPrimary; x: 24; y: 8; rotation: -32 }
            MouseArea {
                anchors.fill: parent; acceptedButtons: Qt.RightButton
                onClicked: appSettings.preferredMeasurementTool =
                           appSettings.preferredMeasurementTool === 1 ? 2 : 1
            }
        }
        Rectangle { width: 1; height: 28; anchors.verticalCenter: parent.verticalCenter; color: themeManager.border }

        Loader {
            anchors.verticalCenter: parent.verticalCenter
            active: root.gridModel.activePane !== null
            sourceComponent: TimeControls { paneController: root.gridModel.activePane }
        }

        ToolButton {
            visible: root.gridModel.activePane !== null
            width: 92
            hint: "Radar product"
            Text {
                anchors.centerIn: parent
                width: parent.width - 8; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                text: root.gridModel.activePane ? root.gridModel.activePane.productName : ""
                color: themeManager.textSecondary; font.pixelSize: 9
            }
            onTriggered: {
                const pane = root.gridModel.activePane
                const products = pane.availableProducts
                pane.productName = products[(products.indexOf(pane.productName) + 1) % products.length]
            }
        }
        ToolButton {
            visible: root.gridModel.activePane !== null
            width: 45
            hint: "Elevation tilt"
            Text {
                anchors.centerIn: parent
                text: root.gridModel.activePane
                    ? Number(root.gridModel.activePane.selectedElevation).toFixed(1) + "°" : "—"
                color: themeManager.textSecondary; font.pixelSize: 9
            }
            onTriggered: {
                const pane = root.gridModel.activePane
                const cuts = pane.elevationCuts
                if (cuts.length === 0) return
                var current = 0
                for (var i = 0; i < cuts.length; ++i)
                    if (Math.abs(cuts[i] - pane.selectedElevation) < 0.01) current = i
                pane.selectedElevation = cuts[(current + 1) % cuts.length]
            }
        }

        ToolButton {
            visible: root.gridModel.gridWidth * root.gridModel.gridHeight > 1
            onTriggered: root.gridModel.setActivePaneIndex(
                (root.gridModel.activePaneIndex + 1) %
                (root.gridModel.gridWidth * root.gridModel.gridHeight))
            Text { anchors.centerIn: parent; text: "P" + (root.gridModel.activePaneIndex + 1); color: themeManager.textSecondary; font.pixelSize: 10; font.bold: true }
        }

        Rectangle { width: 1; height: 28; anchors.verticalCenter: parent.verticalCenter; color: themeManager.border }

        ToolButton {
            id: layoutButton
            active: root.layoutOpen
            width: 38
            hint: "Pane layout. Activate to toggle the last layout; hold or press Down to choose a layout"
            onTriggered: root.toggleLastLayout()
            onHeld: {
                root.layoutOpen = true
                root.analysisOpen = false
            }

            Keys.onDownPressed: {
                root.layoutOpen = true
                root.analysisOpen = false
            }

            Item {
                width: 22
                height: 22
                anchors.centerIn: parent

                Repeater {
                    model: root.gridModel.gridWidth * root.gridModel.gridHeight
                    Rectangle {
                        required property int index
                        readonly property int cols: root.gridModel.gridWidth
                        readonly property int rows: root.gridModel.gridHeight
                        width: (22 - (cols - 1) * 2) / cols
                        height: (22 - (rows - 1) * 2) / rows
                        x: (index % cols) * (width + 2)
                        y: Math.floor(index / cols) * (height + 2)
                        color: layoutButton.active ? themeManager.primary : themeManager.textSecondary
                        radius: 1
                    }
                }
            }
        }

        }
    }
}

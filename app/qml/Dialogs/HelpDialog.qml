// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    anchors.fill: parent
    visible: false
    z: 120
    color: "#99000000"
    focus: visible
    property int page: 0

    function open() {
        visible = true
        forceActiveFocus()
    }
    function close() { visible = false }

    Keys.onEscapePressed: (event) => {
        close()
        event.accepted = true
    }

    MouseArea {
        anchors.fill: parent
        preventStealing: true
        onClicked: root.close()
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(700, root.width - 40)
        height: Math.min(650, root.height - 40)
        radius: themeManager.cornerRadius
        color: themeManager.surface
        border.color: themeManager.border

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onWheel: (wheel) => wheel.accepted = true
        }

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Row {
                width: parent.width
                Text {
                    text: "WxLens help and shortcuts"
                    color: themeManager.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }
                Item { width: Math.max(0, parent.width - 245); height: 1 }
                WxButton {
                    width: 28; height: 28; flat: true
                    text: "×"; name: "Close help"
                    onClicked: root.close()
                }
            }

            Row {
                spacing: 6
                Repeater {
                    model: ["Getting started", "Shortcuts", "Products and palettes"]
                    WxButton {
                        required property string modelData
                        required property int index
                        text: modelData
                        name: modelData + " help page"
                        highlighted: root.page === index
                        height: 28
                        onClicked: root.page = index
                    }
                }
            }

            ScrollView {
                id: helpScroll
                width: parent.width
                height: parent.height - 85
                clip: true
                contentWidth: availableWidth

                Text {
                    width: helpScroll.availableWidth
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondary
                    font.pixelSize: 12
                    lineHeight: 1.28
                    textFormat: Text.RichText
                    text: root.page === 0 ?
                          "<h3>What WxLens is for</h3>" +
                          "WxLens is a multi-pane weather-radar workstation. Each pane can use " +
                          "its own radar site, product, tilt and palette, or selected properties " +
                          "can be synchronized between panes.<br><br>" +
                          "<h3>Getting started</h3>" +
                          "1. Click a pane to make it active.<br>" +
                          "2. Use the pane’s site control to choose a radar.<br>" +
                          "3. Open Products to choose Reflectivity, Velocity, or an available Level 3 product.<br>" +
                          "4. Use the bottom bar for time, measurement, layout and active-pane controls.<br>" +
                          "5. Use each pane’s Unlinked button to place panes in Link A or Link B." :
                          root.page === 1 ?
                          "<h3>Keyboard shortcuts</h3>" +
                          "<b>F1</b> or <b>Ctrl+/</b> — open this help<br>" +
                          "<b>Ctrl+,</b> — open Settings<br>" +
                          "<b>Escape</b> — close the active picker or dialog<br>" +
                          "<b>Ctrl+F</b> — open Saved Places and focus its search<br><br>" +
                          "<h3>Map and tools</h3>" +
                          "Drag to pan and use the wheel to zoom. While a measurement or drawing " +
                          "tool is active, hold <b>Shift</b> to pan the map. Hold <b>Alt</b> to " +
                          "temporarily suppress snapping. Right-click finishes a path measurement " +
                          "or cancels a line drawing." :
                          "<h3>Palettes</h3>" +
                          "Choose and edit palettes in the Palette manager, then press Apply to product. " +
                          "That updates every pane displaying a compatible product; its top buttons choose " +
                          "which palette is being edited. Import a .pal file there when you want to " +
                          "work with an external palette.<br><br>" +
                          "<h3>Storm tracking</h3>" +
                          "Choose an available Level 3 Storm Tracking Information product. Storm " +
                          "IDs and tracks appear over the map; click close to a storm ID or track " +
                          "point to highlight that storm. NST will not be listed when the live " +
                          "provider has no current NST file for that site."
                }
            }
        }
    }
}

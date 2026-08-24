// SPDX-License-Identifier: MIT
import QtQuick

// The settings surface (docs/ROADMAP.md §3.2, §4.5, slice 17).
//
// **Sections are addressed by stable id, not by index.** §4.5 requires a quick control to be able
// to open Settings "already scrolled to and highlighting the specific section that sets its
// default", and warns that retrofitting addressability is the expensive path - so `openAt(id)` is
// the primary entry point and plain `open()` is the degenerate case of it. The id vocabulary comes
// from settings::AppSettings::sections(), which is also what the C++ side validates against, so a
// stale link cannot silently land on a blank panel.
//
// Hand-rolled from plain QtQuick because the app does not depend on QtQuick.Controls yet; slice 9's
// shared Controls/ style is the right place to restyle this, not to reinvent it.
Item {
    id: root
    anchors.fill: parent
    visible: false

    // Which section is showing. Always a valid id while visible.
    property string currentSection: "measurement"

    // Briefly outlined after a deep-link, so arriving from a quick control lands the eye on the
    // section that was asked for rather than on a page that merely contains it.
    property string highlightedSection: ""

    readonly property var sections:
        (typeof appSettings !== "undefined" && appSettings !== null) ? appSettings.sections() : []

    function open() {
        root.openAt(root.currentSection)
    }

    function openAt(sectionId) {
        if (typeof appSettings === "undefined" || appSettings === null) {
            return
        }
        // A link from an older build, or a typo, opens the first section rather than nothing at
        // all - the user asked for Settings and should get Settings.
        root.currentSection = appSettings.hasSection(sectionId)
            ? sectionId
            : (root.sections.length > 0 ? root.sections[0].id : "")
        root.highlightedSection = root.currentSection
        root.visible = true
        highlightTimer.restart()
    }

    function close() {
        root.visible = false
        root.highlightedSection = ""
    }

    Timer {
        id: highlightTimer
        interval: 1400
        onTriggered: root.highlightedSection = ""
    }

    // Scrim. Also swallows clicks so nothing behind the dialog reacts while it is open - without
    // this, a click meant for the dialog's backdrop would place a map object.
    Rectangle {
        anchors.fill: parent
        color: "#b3000000"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: root.close()
        }
    }

    Rectangle {
        width: Math.min(720, root.width - 80)
        height: Math.min(520, root.height - 80)
        anchors.centerIn: parent
        radius: 8
        color: "#161b22"
        border.color: "#2f3742"
        border.width: 1

        // Clicks inside the panel must not reach the scrim's dismiss handler.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
        }

        Text {
            id: dialogTitle
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16
            text: "Settings"
            color: "#e8edf2"
            font.pixelSize: 15
            font.bold: true
        }

        Item {
            width: 24
            height: 24
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeArea.containsMouse ? "#e8edf2" : "#8d99a8"
                font.pixelSize: 13
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.close()
            }
        }

        // ---- section list -------------------------------------------------------------------
        Column {
            id: sectionList
            anchors.left: parent.left
            anchors.top: dialogTitle.bottom
            anchors.bottom: footer.top
            anchors.margins: 16
            width: 180
            spacing: 2

            Repeater {
                model: root.sections

                delegate: Rectangle {
                    required property var modelData

                    width: sectionList.width
                    height: 34
                    radius: 4
                    color: root.currentSection === modelData.id
                        ? "#243040"
                        : (sectionArea.containsMouse ? "#1c232c" : "transparent")
                    border.color: root.highlightedSection === modelData.id ? "#4a7ab0" : "transparent"
                    border.width: 1

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        text: parent.modelData.title
                        color: root.currentSection === parent.modelData.id ? "#dce6f2" : "#8d99a8"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: sectionArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentSection = parent.modelData.id
                    }
                }
            }
        }

        Rectangle {
            anchors.left: sectionList.right
            anchors.leftMargin: 8
            anchors.top: sectionList.top
            anchors.bottom: sectionList.bottom
            width: 1
            color: "#2a3138"
        }

        // ---- section content ----------------------------------------------------------------
        Flickable {
            id: content
            anchors.left: sectionList.right
            anchors.leftMargin: 25
            anchors.right: parent.right
            anchors.top: sectionList.top
            anchors.bottom: footer.top
            anchors.rightMargin: 16
            clip: true
            contentHeight: contentColumn.height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: contentColumn
                width: content.width
                spacing: 14

                Text {
                    width: parent.width
                    text: {
                        for (var i = 0; i < root.sections.length; ++i) {
                            if (root.sections[i].id === root.currentSection) {
                                return root.sections[i].summary
                            }
                        }
                        return ""
                    }
                    color: "#7b8794"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                // -- Measurement --------------------------------------------------------------
                SettingsChoice {
                    visible: root.currentSection === "measurement"
                    width: contentColumn.width
                    label: "Starting a measurement"
                    explanation: "Both is the default. Drag only makes a stray click inert; " +
                                 "click to start/finish suits a pointer you would rather not hold down."
                    options: ["Drag or click", "Drag only", "Click to start, click to finish"]
                    currentIndex: (typeof appSettings !== "undefined" && appSettings !== null)
                        ? appSettings.measurementGesture : 0
                    onSelected: (index) => appSettings.measurementGesture = index
                }

                // -- Map objects --------------------------------------------------------------
                SettingsChoice {
                    visible: root.currentSection === "objects"
                    width: contentColumn.width
                    label: "Default scope for new objects"
                    explanation: "Where a marker, ring or pinned measurement appears when you " +
                                 "create it. This pane only is the shipped default; the per-object " +
                                 "control in the rail still overrides it for one object."
                    // MapObjectScopeKind: 0 CurrentPaneOnly, 1 SyncGroup, 3 AllPanes. SameLocation
                    // (2) is omitted because it has no UI to select it yet.
                    options: ["This pane only", "Panes linked to it", "All panes"]
                    currentIndex: {
                        if (typeof appSettings === "undefined" || appSettings === null) {
                            return 0
                        }
                        const kinds = [0, 1, 3]
                        const at = kinds.indexOf(appSettings.defaultObjectScope)
                        return at < 0 ? 0 : at
                    }
                    onSelected: (index) => appSettings.defaultObjectScope = [0, 1, 3][index]
                }

                // -- Units --------------------------------------------------------------------
                SettingsChoice {
                    visible: root.currentSection === "units"
                    width: contentColumn.width
                    label: "Distances and altitudes"
                    explanation: "Both shows metric and customary together, which is what the " +
                                 "measurement readout did before this setting existed."
                    options: ["Both", "Metric (km, m)", "Imperial (mi, ft)"]
                    currentIndex: (typeof appSettings !== "undefined" && appSettings !== null)
                        ? appSettings.distanceUnits : 0
                    onSelected: (index) => appSettings.distanceUnits = index
                }

                // -- Radar geometry -----------------------------------------------------------
                Column {
                    visible: root.currentSection === "radar-geometry"
                    width: contentColumn.width
                    spacing: 8

                    Text {
                        text: "Rows shown in the beam-height readout"
                        color: "#dce6f2"
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: (typeof appSettings !== "undefined" && appSettings !== null)
                            ? appSettings.geometryRows : []

                        delegate: Column {
                            id: rowEntry
                            required property var modelData

                            width: contentColumn.width - 20
                            spacing: 2

                            // The checkbox row is an Item with the MouseArea filling it, not a
                            // Column sibling: a MouseArea placed directly in the Column would be
                            // laid out as another row below the checkbox rather than over it, and
                            // would take clicks nowhere near the thing it toggles.
                            Item {
                                width: rowEntry.width
                                height: 20

                                Rectangle {
                                    id: checkBox
                                    width: 14
                                    height: 14
                                    radius: 3
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: rowEntry.modelData.visible ? "#2b3a4d" : "#1a1f26"
                                    border.color: rowEntry.modelData.visible ? "#4a7ab0" : "#2f3742"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        visible: rowEntry.modelData.visible
                                        text: "✓"
                                        color: "#dce6f2"
                                        font.pixelSize: 10
                                    }
                                }

                                Text {
                                    anchors.left: checkBox.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: rowEntry.modelData.label
                                    color: "#dce6f2"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appSettings.setGeometryRowVisible(
                                        rowEntry.modelData.id, !rowEntry.modelData.visible)
                                }
                            }

                            // Why a row defaults on, where that is not obvious - the terrain and
                            // AGL rows exist precisely so an MSL figure is never mistaken for an
                            // AGL one (§4.7), and a user turning them off should see that first.
                            Text {
                                visible: rowEntry.modelData.note !== ""
                                x: 22
                                width: rowEntry.width - 22
                                text: rowEntry.modelData.note
                                color: "#5f6b7a"
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                                bottomPadding: 4
                            }
                        }
                    }
                }
            }
        }

        // ---- footer ---------------------------------------------------------------------------
        Item {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            height: 30

            // The config directory, shown because a hand-editable file is only useful if it can be
            // found (ADR 0003 chose TOML specifically so these files are readable and shareable).
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 120
                elide: Text.ElideMiddle
                text: (typeof appSettings !== "undefined" && appSettings !== null)
                    ? appSettings.configDirectory() : ""
                color: "#4f5a68"
                font.pixelSize: 9
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 108
                height: 24
                radius: 4
                color: resetArea.containsMouse ? "#2a2230" : "#20262e"
                border.color: "#2f3742"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Reset to defaults"
                    color: "#8d99a8"
                    font.pixelSize: 10
                }

                MouseArea {
                    id: resetArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: appSettings.resetToDefaults()
                }
            }
        }
    }
}

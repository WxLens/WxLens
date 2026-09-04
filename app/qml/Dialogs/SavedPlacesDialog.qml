// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Dialogs
import "../Controls"

Rectangle {
    id: root
    anchors.fill: parent; visible: false; z: 100; color: "#99000000"
    focus: visible
    property string selectedGroup: savedPlaces.groups.length ? savedPlaces.groups[0].id : ""
    property int selectedPlace: -1
    property string placeOverride: ""
    function open() {
        visible = true
        forceActiveFocus()
        if (selectedGroup !== "") {
            for (var i = 0; i < savedPlaces.groups.length; ++i) {
                if (savedPlaces.groups[i].id === selectedGroup) {
                    selectGroup(savedPlaces.groups[i])
                    break
                }
            }
        }
    }
    function openAndFocusSearch() {
        open()
        Qt.callLater(function() {
            search.forceActiveFocus()
            search.selectAll()
        })
    }
    function close() { visible = false }
    Keys.onEscapePressed: (event) => {
        close()
        event.accepted = true
    }
    Keys.onPressed: (event) => {
        if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_F) {
            search.forceActiveFocus()
            search.selectAll()
            event.accepted = true
        }
    }
    function selectGroup(group) {
        selectedGroup = group.id
        groupName.text = group.name
        groupColor.colorValue = group.color
    }
    function selectPlace(place) {
        selectedPlace = place.id
        selectedGroup = place.groupId
        placeName.text = place.name
        placeLat.text = String(place.latitude)
        placeLon.text = String(place.longitude)
        placeOverride = place.colorOverride
        placeColor.colorValue = place.colorOverride === "" ? place.color : place.colorOverride
    }
    function clearPlaceEditor() {
        selectedPlace = -1; placeName.text = ""; placeLat.text = ""; placeLon.text = ""
        placeOverride = ""; placeColor.colorValue = "#ffb300"
    }
    MouseArea { anchors.fill: parent; onClicked: root.close() }

    Rectangle {
        anchors.centerIn: parent; width: Math.min(820, root.width - 40); height: Math.min(650, root.height - 40)
        radius: themeManager.cornerRadius; color: themeManager.surface; border.color: themeManager.border
        MouseArea { anchors.fill: parent }
        DialogCloseButton {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            onClicked: root.close()
        }
        Column {
            anchors.fill: parent; anchors.margins: 18; spacing: 10
            Row {
                width: parent.width - 30; spacing: 16
                Text { text: "Saved places"; color: themeManager.textPrimary; font.pixelSize: 18; font.bold: true }
                Item { width: parent.width - 330; height: 1 }
                Text { text: "Import"; color: themeManager.accent; MouseArea { anchors.fill: parent; onClicked: importDialog.open() } }
                Text { text: "Export"; color: themeManager.accent; MouseArea { anchors.fill: parent; onClicked: exportDialog.open() } }
            }
            Rectangle {
                width: parent.width; height: 32; radius: 4; color: themeManager.control
                TextInput { id: search; anchors.fill: parent; anchors.margins: 8; color: themeManager.textPrimary; selectByMouse: true
                    Text { visible: search.text === ""; text: "Search places"; color: themeManager.textMuted }
                }
            }
            Row {
                width: parent.width; height: parent.height - 100; spacing: 14
                Column {
                    width: 230; height: parent.height; spacing: 8
                    Text { text: "Groups"; color: themeManager.textSecondary; font.bold: true }
                    ListView {
                        width: parent.width; height: parent.height - 190; clip: true; model: savedPlaces.groups
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 34; radius: 4
                            color: root.selectedGroup === modelData.id ? themeManager.controlActive : "transparent"
                            Rectangle { x: 7; anchors.verticalCenter: parent.verticalCenter; width: 12; height: 12; radius: 6; color: modelData.color }
                            Text { x: 27; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: themeManager.textPrimary }
                            Text { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.visible ? "●" : "○"; color: themeManager.textMuted
                                MouseArea { anchors.fill: parent; onClicked: savedPlaces.setGroupVisible(modelData.id, !modelData.visible) } }
                            MouseArea { anchors.fill: parent; z: -1; onClicked: root.selectGroup(modelData) }
                        }
                    }
                    Rectangle { width: parent.width; height: 30; color: themeManager.control; radius: 4
                        TextInput { id: groupName; anchors.fill: parent; anchors.margins: 7; color: themeManager.textPrimary; Text { visible: groupName.text === ""; text: "New group name"; color: themeManager.textMuted } }
                    }
                    ColorPicker { id: groupColor; width: parent.width; height: 105; colorValue: "#ffb300" }
                    Row { spacing: 16
                        Text { text: root.selectedGroup === "" ? "+ Add group" : "Save group"; color: themeManager.accent
                            MouseArea { anchors.fill: parent; onClicked: {
                                if (root.selectedGroup === "") {
                                    var id = savedPlaces.addGroup(groupName.text, String(groupColor.colorValue))
                                    if (id !== "") root.selectedGroup = id
                                } else {
                                    savedPlaces.editGroup(root.selectedGroup, groupName.text, String(groupColor.colorValue))
                                }
                            } }
                        }
                        Text { text: "New"; color: themeManager.textSecondary
                            MouseArea { anchors.fill: parent; onClicked: { root.selectedGroup=""; groupName.text=""; groupColor.colorValue="#ffb300" } }
                        }
                        Text { visible: root.selectedGroup !== ""; text: "Delete group"; color: themeManager.danger
                            MouseArea { anchors.fill: parent; onClicked: { savedPlaces.removeGroup(root.selectedGroup); root.selectedGroup=""; groupName.text=""; root.clearPlaceEditor() } }
                        }
                    }
                }
                Column {
                    width: parent.width - 244; height: parent.height; spacing: 8
                    Text { text: "Places"; color: themeManager.textSecondary; font.bold: true }
                    ListView {
                        width: parent.width; height: parent.height - (root.placeOverride === "" ? 125 : 230); clip: true; model: savedPlaces.search(search.text)
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 42; color: index % 2 ? themeManager.elevatedSurface : "transparent"
                            Rectangle { x: 6; anchors.verticalCenter: parent.verticalCenter; width: 12; height: 12; radius: 6; color: modelData.color }
                            Column { x: 28; anchors.verticalCenter: parent.verticalCenter
                                Text { text: modelData.name; color: themeManager.textPrimary }
                                Text { text: Number(modelData.latitude).toFixed(4) + ", " + Number(modelData.longitude).toFixed(4); color: themeManager.textMuted; font.pixelSize: 10 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.selectPlace(modelData) }
                        }
                    }
                    Row { spacing: 6
                        Rectangle { width: 160; height: 30; color: themeManager.control; radius: 4
                            TextInput { id: placeName; anchors.fill: parent; anchors.margins: 7; color: themeManager.textPrimary; selectByMouse: true; Text { visible: placeName.text === ""; text: "Name"; color: themeManager.textMuted } } }
                        Rectangle { width: 110; height: 30; color: themeManager.control; radius: 4
                            TextInput { id: placeLat; anchors.fill: parent; anchors.margins: 7; color: themeManager.textPrimary; selectByMouse: true; Text { visible: placeLat.text === ""; text: "Latitude"; color: themeManager.textMuted } } }
                        Rectangle { width: 110; height: 30; color: themeManager.control; radius: 4
                            TextInput { id: placeLon; anchors.fill: parent; anchors.margins: 7; color: themeManager.textPrimary; selectByMouse: true; Text { visible: placeLon.text === ""; text: "Longitude"; color: themeManager.textMuted } } }
                    }
                    Row { spacing: 10
                        Text { text: root.placeOverride === "" ? "Use group color" : "Color override"; color: themeManager.textSecondary; font.pixelSize: 10 }
                        Rectangle { width: 16; height: 16; radius: 8; color: placeColor.colorValue; border.color: themeManager.border }
                        Text { text: root.placeOverride === "" ? "Override" : "Use group"; color: themeManager.accent; font.pixelSize: 10
                            MouseArea { anchors.fill: parent; onClicked: root.placeOverride = root.placeOverride === "" ? String(placeColor.colorValue) : "" }
                        }
                    }
                    ColorPicker { id: placeColor; visible: root.placeOverride !== ""; width: parent.width; height: 105; colorValue: "#ffb300"
                        onColorEdited: (value) => root.placeOverride = String(value)
                    }
                    Row { spacing: 18
                        Text { text: root.selectedPlace < 0 ? "+ Add to selected group" : "Save place"; color: root.selectedGroup === "" ? themeManager.textMuted : themeManager.accent
                            MouseArea { anchors.fill: parent; enabled: root.selectedGroup !== ""; onClicked: {
                                var ok = root.selectedPlace < 0
                                    ? savedPlaces.addPlace(placeName.text, Number(placeLat.text), Number(placeLon.text), root.selectedGroup, root.placeOverride) >= 0
                                    : savedPlaces.editPlace(root.selectedPlace, placeName.text, Number(placeLat.text), Number(placeLon.text), root.selectedGroup, root.placeOverride)
                                if (ok) root.clearPlaceEditor()
                            } }
                        }
                        Text { text: "New"; color: themeManager.textSecondary; MouseArea { anchors.fill: parent; onClicked: root.clearPlaceEditor() } }
                        Text { visible: root.selectedPlace >= 0; text: "Delete place"; color: themeManager.danger
                            MouseArea { anchors.fill: parent; onClicked: { savedPlaces.removePlace(root.selectedPlace); root.clearPlaceEditor() } }
                        }
                    }
                }
            }
        }
    }
    FileDialog { id: importDialog; title: "Import saved places"; nameFilters: ["WxLens saved places (*.json)"]; onAccepted: savedPlaces.importFile(selectedFile) }
    FileDialog { id: exportDialog; title: "Export saved places"; fileMode: FileDialog.SaveFile; nameFilters: ["WxLens saved places (*.json)"]; onAccepted: savedPlaces.exportFile(selectedFile) }
}

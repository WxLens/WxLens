// SPDX-License-Identifier: MIT
import QtQuick

Rectangle {
    id: root
    height: 40
    color: themeManager.surface
    z: 100
    signal settingsRequested()
    signal paletteRequested()
    signal mapDetailsRequested()
    signal savedPlacesRequested()
    signal overlaysRequested()
    signal helpRequested()
    property bool toolsOpen: false

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: themeManager.border }
    Text {
        anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
        text: "WxLens"; color: themeManager.textPrimary; font.pixelSize: 14; font.bold: true
    }
    Row {
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 6
        Repeater {
            model: appSettings.toolbarActions.filter(function(action) { return action.visible })
            delegate: Rectangle {
                required property var modelData
                width: shortcutLabel.implicitWidth + 16; height: 28; radius: themeManager.cornerRadius
                color: shortcutArea.containsMouse ? themeManager.controlHover : themeManager.control
                border.color: themeManager.border
                Text { id: shortcutLabel; anchors.centerIn: parent; text: parent.modelData.label; color: themeManager.textSecondary; font.pixelSize: 10 }
                MouseArea {
                    id: shortcutArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (parent.modelData.id === "overlays") root.overlaysRequested()
                        else if (parent.modelData.id === "places") root.savedPlacesRequested()
                        else if (parent.modelData.id === "map") root.mapDetailsRequested()
                        else root.paletteRequested()
                    }
                }
            }
        }
        Rectangle {
            width: 62; height: 28; radius: themeManager.cornerRadius
            color: toolsArea.containsMouse ? themeManager.controlHover : themeManager.control
            border.color: root.toolsOpen ? themeManager.primary : themeManager.border
            Text { anchors.centerIn: parent; text: "Tools ⋯"; color: themeManager.textSecondary; font.pixelSize: 10 }
            MouseArea { id: toolsArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.toolsOpen = !root.toolsOpen }
        }
        Rectangle {
            width: 28; height: 28; radius: themeManager.cornerRadius
            color: helpArea.containsMouse ? themeManager.controlHover : "transparent"
            Text { anchors.centerIn: parent; text: "?"; color: themeManager.textSecondary; font.pixelSize: 14; font.bold: true }
            MouseArea { id: helpArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.helpRequested() }
        }
        Rectangle {
            width: 28; height: 28; radius: themeManager.cornerRadius
            color: settingsArea.containsMouse ? themeManager.controlHover : "transparent"
            Text { anchors.centerIn: parent; text: "⚙"; color: themeManager.textSecondary; font.pixelSize: 16 }
            MouseArea { id: settingsArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settingsRequested() }
        }
    }
    Rectangle {
        visible: root.toolsOpen
        anchors.right: parent.right; anchors.rightMargin: 48; anchors.top: parent.bottom
        width: 210; height: menuColumn.implicitHeight + 16
        radius: themeManager.cornerRadius; color: themeManager.elevatedSurface; border.color: themeManager.border; z: 200
        Column {
            id: menuColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; spacing: 3
            Repeater {
                model: [{label:"Weather overlays",action:"overlays"},{label:"Saved places",action:"places"},{label:"Map details",action:"map"},{label:"Palette manager",action:"palette"},{label:"Help and shortcuts",action:"help"}]
                delegate: Rectangle {
                    required property var modelData
                    width: menuColumn.width; height: 34; radius: themeManager.cornerRadius
                    color: entryArea.containsMouse ? themeManager.controlHover : "transparent"
                    Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: parent.modelData.label; color: themeManager.textPrimary; font.pixelSize: 11 }
                    MouseArea {
                        id: entryArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.toolsOpen = false
                            if (parent.modelData.action === "overlays") root.overlaysRequested()
                            else if (parent.modelData.action === "places") root.savedPlacesRequested()
                            else if (parent.modelData.action === "map") root.mapDetailsRequested()
                            else if (parent.modelData.action === "palette") root.paletteRequested()
                            else root.helpRequested()
                        }
                    }
                }
            }
        }
    }
}

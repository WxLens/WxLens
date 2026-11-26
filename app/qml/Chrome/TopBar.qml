// SPDX-License-Identifier: MIT
import QtQuick

import WxLens.App

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
    signal experienceSettingsRequested()
    property bool toolsOpen: false

    Accessible.role: Accessible.ToolBar
    Accessible.name: "Application toolbar"

    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: themeManager.border }
    Text {
        anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
        text: "WxLens"; color: themeManager.textPrimary; font.pixelSize: 14; font.bold: true
    }
    Row {
        anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 6
        // Experience level (ADR 0005 §5): the one deliberate rarely-changed-mode exception to the
        // minimal-chrome rule, because switching quickly is the point. One control; it shortens to
        // the level's initial when the window is narrow.
        WxButton {
            id: levelButton
            readonly property string levelName: appSettings.experienceLevelNames[appSettings.experienceLevel]
            readonly property bool compact: root.width < 720
            width: compact ? 40 : implicitWidth
            text: (compact ? levelName.charAt(0) : levelName +
                   (appSettings.experienceCustomized ? " (customized)" : "")) + " ▾"
            font.pixelSize: 10
            name: "Experience level"
            description: levelName + (appSettings.experienceCustomized ? ", customized" : "") +
                         ". Choose how much of WxLens is shown."
            // The tooltip would sit on top of the menu it opens, covering its first item.
            tooltip: levelMenu.visible ? "" : description
            highlighted: levelMenu.visible
            onClicked: { levelMenu.visible = !levelMenu.visible; root.toolsOpen = false }
        }
        Repeater {
            model: appSettings.toolbarActions.filter(function(action) { return action.visible })
            delegate: WxButton {
                required property var modelData
                text: modelData.label
                font.pixelSize: 10
                description: "Open " + modelData.label
                onClicked: {
                    if (modelData.id === "overlays") root.overlaysRequested()
                    else if (modelData.id === "places") root.savedPlacesRequested()
                    else if (modelData.id === "map") root.mapDetailsRequested()
                    else root.paletteRequested()
                }
            }
        }
        WxButton {
            width: 62
            text: "Tools ⋯"
            font.pixelSize: 10
            name: "Tools"
            description: "Weather overlays, saved places, map details, palettes and help"
            highlighted: root.toolsOpen
            onClicked: { root.toolsOpen = !root.toolsOpen; levelMenu.visible = false }
        }
        WxButton {
            width: 28
            flat: true
            text: "?"
            font.pixelSize: 14
            font.bold: true
            // The glyph is meaningless read aloud, so the spoken name is spelled out.
            name: "Help and shortcuts"
            onClicked: root.helpRequested()
        }
        WxButton {
            width: 28
            flat: true
            text: "⚙"
            font.pixelSize: 16
            name: "Settings"
            onClicked: root.settingsRequested()
        }
    }
    Rectangle {
        id: levelMenu
        visible: false
        x: Math.max(8, Math.min(root.width - width - 8,
                                levelButton.mapToItem(root, 0, 0).x))
        anchors.top: parent.bottom
        width: 230; height: levelColumn.implicitHeight + 16
        radius: themeManager.cornerRadius; color: themeManager.elevatedSurface; border.color: themeManager.border; z: 200
        Accessible.role: Accessible.PopupMenu
        Accessible.name: "Experience level menu"
        Column {
            id: levelColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; spacing: 3
            Repeater {
                model: appSettings.experienceLevelNames
                delegate: WxMenuItem {
                    required property var modelData
                    required property int index
                    width: levelColumn.width
                    text: modelData
                    selected: index === appSettings.experienceLevel
                    onTriggered: {
                        levelMenu.visible = false
                        appSettings.experienceLevel = index
                    }
                }
            }
            WxMenuItem {
                visible: appSettings.experienceCustomized
                width: levelColumn.width
                text: "Reset to " + levelButton.levelName + " defaults"
                onTriggered: { levelMenu.visible = false; appSettings.resetExperienceOverrides() }
            }
            Rectangle { width: levelColumn.width; height: 1; color: themeManager.border }
            WxMenuItem {
                width: levelColumn.width
                text: "Customize…"
                onTriggered: { levelMenu.visible = false; root.experienceSettingsRequested() }
            }
        }
    }
    Rectangle {
        visible: root.toolsOpen
        anchors.right: parent.right; anchors.rightMargin: 48; anchors.top: parent.bottom
        width: 210; height: menuColumn.implicitHeight + 16
        radius: themeManager.cornerRadius; color: themeManager.elevatedSurface; border.color: themeManager.border; z: 200
        Accessible.role: Accessible.PopupMenu
        Accessible.name: "Tools menu"
        Column {
            id: menuColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; spacing: 3
            Repeater {
                model: [{label:"Weather overlays",action:"overlays"},{label:"Saved places",action:"places"},{label:"Map details",action:"map"},{label:"Palette manager",action:"palette"},{label:"Help and shortcuts",action:"help"}]
                delegate: WxMenuItem {
                    required property var modelData
                    width: menuColumn.width
                    text: modelData.label
                    onTriggered: {
                        root.toolsOpen = false
                        if (modelData.action === "overlays") root.overlaysRequested()
                        else if (modelData.action === "places") root.savedPlacesRequested()
                        else if (modelData.action === "map") root.mapDetailsRequested()
                        else if (modelData.action === "palette") root.paletteRequested()
                        else root.helpRequested()
                    }
                }
            }
        }
    }
}

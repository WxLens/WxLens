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
            onClicked: root.toolsOpen = !root.toolsOpen
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

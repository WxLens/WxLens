// SPDX-License-Identifier: MIT
import QtQuick

// The site/status text is real (radarStatus context property,
// wxlens::products::RadarProductStatus, slice 2) but this is still a single hardcoded site, not
// the real per-pane product binding that arrives with PaneGridModel/PaneController (slice 4+).
Rectangle {
    id: root
    height: 48
    color: themeManager.surface

    // The plain "open Settings" route. §4.5's deep-links are the other direction and come from
    // the individual quick controls, not from here.
    signal settingsRequested()
    signal paletteRequested()
    signal mapDetailsRequested()
    signal savedPlacesRequested()
    signal overlaysRequested()

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: themeManager.border
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: "WxLens"
        color: themeManager.textPrimary
        font.pixelSize: 16
        font.bold: true
    }

    Item {
        width: 28
        height: 28
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter

        Text {
            anchors.centerIn: parent
            text: "⚙"
            color: settingsArea.containsMouse ? themeManager.textPrimary : themeManager.textMuted
            font.pixelSize: 16
        }

        MouseArea {
            id: settingsArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.settingsRequested()
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 48
        anchors.verticalCenter: parent.verticalCenter
        width: paletteText.implicitWidth + 20
        height: 28
        radius: themeManager.cornerRadius
        color: paletteArea.containsMouse ? themeManager.controlHover : themeManager.control
        border.color: themeManager.border

        Text {
            id: paletteText
            anchors.centerIn: parent
            text: (typeof paletteManager !== "undefined" && paletteManager !== null)
                ? paletteManager.activeName : "Palette"
            color: themeManager.textSecondary
            font.pixelSize: 11
        }
        MouseArea {
            id: paletteArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.paletteRequested()
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 174
        anchors.verticalCenter: parent.verticalCenter
        width: mapDetailsText.implicitWidth + 20
        height: 28
        radius: themeManager.cornerRadius
        color: mapDetailsArea.containsMouse ? themeManager.controlHover : themeManager.control
        border.color: themeManager.border

        Text {
            id: mapDetailsText
            anchors.centerIn: parent
            text: "Map details"
            color: themeManager.textSecondary
            font.pixelSize: 11
        }
        MouseArea {
            id: mapDetailsArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.mapDetailsRequested()
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 278
        anchors.verticalCenter: parent.verticalCenter
        width: placesText.implicitWidth + 20; height: 28; radius: themeManager.cornerRadius
        color: placesArea.containsMouse ? themeManager.controlHover : themeManager.control
        border.color: themeManager.border
        Text { id: placesText; anchors.centerIn: parent; text: "Saved places"; color: themeManager.textSecondary; font.pixelSize: 11 }
        MouseArea { id: placesArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.savedPlacesRequested() }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 390
        anchors.verticalCenter: parent.verticalCenter
        width: overlaysText.implicitWidth + 20; height: 28; radius: themeManager.cornerRadius
        color: overlaysArea.containsMouse ? themeManager.controlHover : themeManager.control
        border.color: themeManager.border
        Text { id: overlaysText; anchors.centerIn: parent; text: "Overlays"; color: themeManager.textSecondary; font.pixelSize: 11 }
        MouseArea { id: overlaysArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.overlaysRequested() }
    }

    Text {
        anchors.centerIn: parent
        // radarStatus is a context property torn down before this item on application exit, so it
        // has to be null-checked or closing the app throws a TypeError here.
        text: (radarStatus !== null && radarStatus !== undefined)
            ? radarStatus.siteId + " — " + radarStatus.statusText
            : ""
        color: themeManager.textMuted
        font.pixelSize: 13
    }
}

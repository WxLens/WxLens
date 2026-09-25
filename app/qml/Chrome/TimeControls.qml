// SPDX-License-Identifier: MIT
import QtQuick

import WxLens.App

Item {
    id: root
    required property var paneController
    width: controls.width
    height: controls.height

    Row {
        id: controls
        anchors.centerIn: parent
        spacing: 6

        WxButton {
            width: 52; height: 32; radius: themeManager.cornerRadius
            text: "LIVE"
            name: "Use live radar data"
            highlighted: root.paneController.liveMode
            onClicked: playback.returnToLive()
        }
        WxButton {
            width: 30; height: 32; text: "‹"; name: "Previous scan"
            enabled: playback.frameCount > 0
            onClicked: playback.step(-1)
        }
        WxButton {
            width: 48; height: 32; text: playback.playing ? "Pause" : "Play"
            name: "Play or pause recent radar history"
            enabled: playback.frameCount > 1
            onClicked: playback.togglePlaying()
        }
        PlaybackScrubber {
            width: 150; height: 32
            frameCount: playback.frameCount
            selectedIndex: playback.selectedIndex
            trackColor: themeManager.border
            handleColor: themeManager.primary
            onSeekRequested: index => playback.seek(index)
        }
        WxButton {
            width: 30; height: 32; text: "›"; name: "Next scan"
            enabled: playback.frameCount > 0
            onClicked: playback.step(1)
        }
        // In the row, not under it: the bottom bar clips its Flickable to the row's height, so a
        // status line anchored below the controls was laid out correctly and never drawn.
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 104; elide: Text.ElideRight
            text: root.paneController.timeError !== "" ? root.paneController.timeError : playback.status
            color: root.paneController.timeError !== "" ? themeManager.danger : themeManager.textMuted
            font.pixelSize: 9
            Accessible.role: Accessible.StaticText
            Accessible.name: "Playback status: " + text
        }
        Rectangle {
            width: 142; height: 32; radius: themeManager.cornerRadius
            color: themeManager.control; border.color: themeManager.border; border.width: 1
            TextInput {
                id: archiveInput
                anchors.fill: parent; anchors.margins: 6
                color: themeManager.textPrimary; selectionColor: themeManager.primary
                font.pixelSize: 11; verticalAlignment: TextInput.AlignVCenter
                text: root.paneController.liveMode
                    ? Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm")
                    : root.paneController.selectedTimeText.replace(" UTC", "")
                onAccepted: root.paneController.selectArchiveTime(text)
                Accessible.role: Accessible.EditableText
                Accessible.name: "Archive date and time in UTC"
            }
        }
        WxButton {
            width: 62; height: 32; radius: themeManager.cornerRadius
            text: root.paneController.timeLoading ? "Loading…" : "Archive"
            name: "Load archive radar data"
            highlighted: !root.paneController.liveMode
            enabled: !root.paneController.timeLoading
            onClicked: { playback.pause(); root.paneController.selectArchiveTime(archiveInput.text) }
        }
        Rectangle {
            width: 62; height: 32; radius: themeManager.cornerRadius
            color: themeManager.control; border.color: themeManager.border; border.width: 1
            TextInput {
                anchors.fill: parent; anchors.margins: 6
                text: root.paneController.sourceKey
                color: themeManager.textPrimary; selectionColor: themeManager.primary
                font.pixelSize: 11; maximumLength: 4; horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                onAccepted: root.paneController.sourceKey = text.toUpperCase()
                Accessible.role: Accessible.EditableText
                Accessible.name: "Radar site identifier"
            }
        }
    }
}

// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls

import WxLens.App

Item {
    id: root
    required property var paneController
    width: 50
    height: 32

    Rectangle {
        id: playButton
        anchors.fill: parent
        radius: themeManager.cornerRadius
        color: playArea.containsMouse ? themeManager.controlHover : themeManager.control
        border.color: playback.playing ? themeManager.primary : themeManager.border
        border.width: root.activeFocus ? 2 : 1
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: playback.playing ? "Pause radar playback" : "Play recent radar history"
        Accessible.description: "Press and hold, or press Down, to open the playback timeline"
        Accessible.onPressAction: playback.togglePlaying()

        Text {
            anchors.centerIn: parent
            text: playback.playing ? "■" : "▶"
            color: playback.playing ? themeManager.primary : themeManager.textPrimary
            font.pixelSize: 13
        }

        MouseArea {
            id: playArea
            property bool heldThisPress: false
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            pressAndHoldInterval: 350
            onPressed: heldThisPress = false
            onPressAndHold: {
                heldThisPress = true
                timelinePopup.open()
            }
            onClicked: {
                if (!heldThisPress) playback.togglePlaying()
            }
        }
        Keys.onSpacePressed: playback.togglePlaying()
        Keys.onReturnPressed: playback.togglePlaying()
        Keys.onDownPressed: timelinePopup.open()
    }

    Popup {
        id: timelinePopup
        parent: root
        x: -Math.max(0, (width - root.width) / 2)
        y: -height - 10
        width: Math.min(650, root.Window.window ? root.Window.window.width - 32 : 650)
        height: 58
        padding: 9
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: themeManager.cornerRadius * 1.4
            color: themeManager.elevatedSurface
            border.color: themeManager.border
            border.width: 1
        }

        contentItem: Row {
            spacing: 6

            WxButton {
                width: 42; height: 32
                text: "LIVE"
                name: "Use live radar data"
                highlighted: root.paneController.liveMode
                onClicked: playback.returnToLive()
            }
            WxButton {
                width: 28; height: 32; text: "‹"; name: "Previous scan"
                enabled: playback.frameCount > 0
                onClicked: playback.step(-1)
            }
            WxButton {
                width: 42; height: 32; text: playback.playing ? "■" : "▶"
                name: "Play or pause recent radar history"
                enabled: playback.frameCount > 1
                onClicked: playback.togglePlaying()
            }
            PlaybackScrubber {
                width: Math.max(120, timelinePopup.availableWidth - 342)
                height: 32
                frameCount: playback.frameCount
                selectedIndex: playback.selectedIndex
                trackColor: themeManager.border
                handleColor: themeManager.primary
                onSeekRequested: index => playback.seek(index)
            }
            WxButton {
                width: 28; height: 32; text: "›"; name: "Next scan"
                enabled: playback.frameCount > 0
                onClicked: playback.step(1)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 82
                elide: Text.ElideRight
                text: root.paneController.timeError !== ""
                    ? root.paneController.timeError : playback.status
                color: root.paneController.timeError !== ""
                    ? themeManager.danger : themeManager.textMuted
                font.pixelSize: 9
                Accessible.role: Accessible.StaticText
                Accessible.name: "Playback status: " + text
            }
        }
    }
}

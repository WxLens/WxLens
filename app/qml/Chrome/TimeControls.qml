// SPDX-License-Identifier: MIT
import QtQuick

Rectangle {
    id: root
    required property var paneController
    width: controls.width + 20
    height: controls.height + 12
    radius: themeManager.cornerRadius
    color: themeManager.elevatedSurface
    border.color: themeManager.border
    border.width: 1
    opacity: 0.96

    Row {
        id: controls
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 52; height: 28; radius: themeManager.cornerRadius
            color: root.paneController.liveMode ? themeManager.controlActive : themeManager.control
            Text { anchors.centerIn: parent; text: "LIVE"; color: themeManager.textPrimary; font.pixelSize: 11 }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.paneController.selectLive() }
        }
        Rectangle {
            width: 142; height: 28; radius: themeManager.cornerRadius
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
            }
        }
        Rectangle {
            width: 62; height: 28; radius: themeManager.cornerRadius
            color: root.paneController.liveMode ? themeManager.control : themeManager.controlActive
            Text { anchors.centerIn: parent; text: root.paneController.timeLoading ? "Loading…" : "Archive"; color: themeManager.textPrimary; font.pixelSize: 10 }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.paneController.selectArchiveTime(archiveInput.text) }
        }
        Rectangle {
            width: 62; height: 28; radius: themeManager.cornerRadius
            color: themeManager.control; border.color: themeManager.border; border.width: 1
            TextInput {
                anchors.fill: parent; anchors.margins: 6
                text: root.paneController.sourceKey
                color: themeManager.textPrimary; selectionColor: themeManager.primary
                font.pixelSize: 11; maximumLength: 4; horizontalAlignment: TextInput.AlignHCenter
                onAccepted: root.paneController.sourceKey = text.toUpperCase()
            }
        }
    }
    Text {
        visible: root.paneController.timeError !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom; anchors.topMargin: 3
        text: root.paneController.timeError
        color: themeManager.danger
        font.pixelSize: 10
    }
}

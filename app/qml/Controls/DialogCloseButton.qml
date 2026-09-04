// SPDX-License-Identifier: MIT
import QtQuick

Item {
    id: root

    signal clicked()

    width: 24
    height: 24

    Accessible.role: Accessible.Button
    Accessible.name: "Close"

    Text {
        anchors.centerIn: parent
        text: "✕"
        color: closeArea.containsMouse ? themeManager.textPrimary : themeManager.textMuted
        font.pixelSize: 13
    }

    MouseArea {
        id: closeArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}

// SPDX-License-Identifier: MIT
import QtQuick

// Phase 1 slice 1: static chrome shell only. This is the successor to
// scwx-qt/ui/radar_toolbox_rail_widget.cpp - real tool icons/actions land alongside the
// features they open (measurement in slice 7, palette editor in slice 9, etc.).
Rectangle {
    id: root
    width: 56
    color: "#14181e"

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: "#2a3138"
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12

        Repeater {
            model: 4

            Rectangle {
                width: 32
                height: 32
                radius: 6
                color: "#20262e"
                border.color: "#2f3742"
                border.width: 1
            }
        }
    }
}

// SPDX-License-Identifier: MIT
import QtQuick

// Phase 1 slice 1: static chrome shell only - no ThemeManager (slice 10) and no live
// site/product/time state (that arrives with the Data Source/Product wiring in slices 2+).
// Colors are hardcoded placeholders here for the same reason.
Rectangle {
    id: root
    height: 48
    color: "#181d24"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#2a3138"
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: "Nimbus"
        color: "#e8edf2"
        font.pixelSize: 16
        font.bold: true
    }

    Text {
        anchors.centerIn: parent
        text: "No site selected"
        color: "#7b8794"
        font.pixelSize: 13
    }
}

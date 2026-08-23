// SPDX-License-Identifier: MIT
import QtQuick

// Phase 1 slice 1/2: static chrome shell - no ThemeManager yet (slice 10). Colors are hardcoded
// placeholders for that reason. The site/status text is real (radarStatus context property,
// nimbus::products::RadarProductStatus, slice 2) but this is still a single hardcoded site, not
// the real per-pane product binding that arrives with PaneGridModel/PaneController (slice 4+).
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
        // radarStatus is a context property torn down before this item on application exit, so it
        // has to be null-checked or closing the app throws a TypeError here.
        text: (radarStatus !== null && radarStatus !== undefined)
            ? radarStatus.siteId + " — " + radarStatus.statusText
            : ""
        color: "#7b8794"
        font.pixelSize: 13
    }
}

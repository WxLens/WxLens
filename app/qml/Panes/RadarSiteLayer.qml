// SPDX-License-Identifier: MIT
import QtQuick

Item {
    id: root
    required property var paneController
    property int cameraTick: 0
    property bool toolsArmed: false
    property real cullMargin: 24
    property real hitTolerance: 12
    property var sites: []

    Accessible.role: Accessible.Pane
    Accessible.name: sites.length + " radar sites visible on this map; use the radar site picker for keyboard selection"

    function refresh() {
        if (!visible || !paneController || typeof radarSiteMarkers === "undefined") {
            sites = []
            return
        }
        sites = radarSiteMarkers.visibleSites(paneController, width, height, cullMargin,
                                              appSettings.tdwrSitesVisible)
    }

    Repeater {
        model: root.sites
        delegate: Item {
            required property var modelData
            x: modelData.x - 5; y: modelData.y - 5
            width: 10; height: 10
            Rectangle {
                anchors.centerIn: parent
                width: modelData.id === root.paneController.sourceKey ? 12 : 8
                height: width; radius: width / 2
                color: modelData.type === "tdwr" ? themeManager.warning : themeManager.primary
                border.width: modelData.id === root.paneController.sourceKey ? 2 : 1
                border.color: themeManager.textPrimary
            }
            Text {
                anchors.left: parent.right; anchors.leftMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                text: root.paneController.zoom >= 9 ? modelData.id + " · " + modelData.name
                      : root.paneController.zoom >= 6 || modelData.id === root.paneController.sourceKey
                        ? modelData.id : ""
                color: themeManager.textPrimary; font.pixelSize: 10; font.bold: true
                style: Text.Outline; styleColor: "#c0000000"
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.visible && !root.toolsArmed
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onClicked: (mouse) => {
            const hit = radarSiteMarkers.nearestSite(root.paneController, mouse.x, mouse.y,
                                                     root.width, root.height, root.cullMargin,
                                                     appSettings.tdwrSitesVisible,
                                                     root.hitTolerance)
            if (hit.id) paneGridModel.selectRadarSite(root.paneController, hit.id)
            else mouse.accepted = false
        }
    }

    onCameraTickChanged: refresh()
    onWidthChanged: refresh()
    onHeightChanged: refresh()
    onVisibleChanged: refresh()
    Connections { target: appSettings; function onTdwrSitesVisibleChanged() { root.refresh() } }
    Component.onCompleted: refresh()
}

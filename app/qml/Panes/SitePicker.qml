// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var paneController
    required property var sites
    signal closeRequested()

    width: Math.min(420, parent ? parent.width - 16 : 420)
    height: Math.min(500, parent ? parent.height - 16 : 500)
    radius: themeManager.cornerRadius
    color: themeManager.elevatedSurface
    border.color: themeManager.border
    z: 30

    property string query: ""
    readonly property var filteredSites: {
        const needle = query.trim().toLowerCase()
        if (needle === "") return sites
        return sites.filter(function(site) {
            return [site.id, site.name, site.region, site.country].join(" ")
                .toLowerCase().indexOf(needle) >= 0
        })
    }

    function choose(site) {
        paneController.sourceKey = site.id
        if (appSettings.centerMapOnSiteChange) {
            paneController.setCenter(site.latitude, site.longitude)
            paneController.zoom = 7.0
        }
        closeRequested()
    }

    // Keep mouse-wheel and touchpad scrolling inside the picker. At either end of the list,
    // ListView can decline the event and otherwise let MapLibre zoom the map underneath.
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            var delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y
                                                  : event.angleDelta.y / 2
            var minimum = siteList.originY
            var maximum = Math.max(minimum,
                                   siteList.originY + siteList.contentHeight - siteList.height)
            siteList.contentY = Math.max(minimum,
                                         Math.min(maximum, siteList.contentY - delta))
            event.accepted = true
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Row {
            width: parent.width
            Text { text: "Radar site"; color: themeManager.textPrimary; font.pixelSize: 16 }
            Item { width: Math.max(0, parent.width - 105); height: 1 }
            Text {
                text: "×"; color: themeManager.textSecondary; font.pixelSize: 18
                MouseArea { anchors.fill: parent; anchors.margins: -7; onClicked: root.closeRequested() }
            }
        }

        TextField {
            id: search
            width: parent.width
            placeholderText: "Search ID, station, city, or state"
            color: themeManager.textPrimary
            placeholderTextColor: themeManager.textMuted
            selectByMouse: true
            background: Rectangle {
                radius: themeManager.cornerRadius
                color: themeManager.control
                border.color: search.activeFocus ? themeManager.primary : themeManager.border
            }
            onTextChanged: root.query = text
            Component.onCompleted: forceActiveFocus()
            Keys.onEscapePressed: root.closeRequested()
        }

        Text {
            text: root.filteredSites.length + " sites"
            color: themeManager.textMuted
            font.pixelSize: 10
        }

        ListView {
            id: siteList
            width: parent.width
            height: parent.height - y
            clip: true
            spacing: 3
            model: root.filteredSites
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 48
                radius: themeManager.cornerRadius
                color: modelData.id === root.paneController.sourceKey ? themeManager.controlActive
                      : area.containsMouse ? themeManager.controlHover : themeManager.control
                border.color: modelData.id === root.paneController.sourceKey
                              ? themeManager.primary : themeManager.border
                Column {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter; anchors.margins: 8
                    Text {
                        text: modelData.id + "  ·  " + modelData.name
                        color: themeManager.textPrimary; font.pixelSize: 12
                        width: parent.width; elide: Text.ElideRight
                    }
                    Text {
                        text: [modelData.region, modelData.country].filter(Boolean).join(", ")
                        color: themeManager.textMuted; font.pixelSize: 10
                    }
                }
                MouseArea {
                    id: area; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.choose(parent.modelData)
                }
            }
        }
    }
}

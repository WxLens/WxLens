// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls

import WxLens.App

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
        const ranked = []
        sites.forEach(function(site, sourceIndex) {
            const id = site.id.toLowerCase()
            const city = site.name.toLowerCase()
            const region = site.region.toLowerCase()
            const regionName = site.regionName.toLowerCase()
            var rank = 99
            if (id.indexOf(needle) >= 0) rank = 0
            else if (city.indexOf(needle) >= 0) rank = 1
            else if (region === needle || regionName === needle) rank = 2
            else if (region.indexOf(needle) >= 0 || regionName.indexOf(needle) >= 0) rank = 3
            else if (site.searchText.indexOf(needle) >= 0) rank = 4
            if (rank !== 99) ranked.push({ "site": site, "rank": rank, "sourceIndex": sourceIndex })
        })
        ranked.sort(function(a, b) {
            if (a.rank !== b.rank) return a.rank - b.rank
            const idOrder = a.site.id.localeCompare(b.site.id)
            return idOrder !== 0 ? idOrder : a.sourceIndex - b.sourceIndex
        })
        return ranked.map(function(entry) { return entry.site })
    }

    function choose(site) {
        paneGridModel.selectRadarSite(paneController, site.id)
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
            WxButton {
                width: 28; height: 28; flat: true
                text: "×"; name: "Close radar site picker"
                onClicked: root.closeRequested()
            }
        }

        TextField {
            id: search
            width: parent.width
            placeholderText: "Search ID, station, city, state, country, or type"
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
            Accessible.name: "Search radar sites"
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
                Accessible.role: Accessible.ListItem
                Accessible.name: modelData.id + ", " + modelData.name + ", " +
                                 modelData.regionName + ", " + modelData.country
                Accessible.onPressAction: root.choose(modelData)
                activeFocusOnTab: true
                Column {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter; anchors.margins: 8
                    Text {
                        text: modelData.id + "  ·  " + modelData.name +
                              (modelData.type === "tdwr" ? "   TDWR" : "")
                        color: themeManager.textPrimary; font.pixelSize: 12
                        width: parent.width; elide: Text.ElideRight
                    }
                    Text {
                        text: [modelData.regionName && modelData.regionName !== modelData.region
                               ? modelData.regionName + " (" + modelData.region + ")"
                               : modelData.region,
                               modelData.country].filter(Boolean).join(", ")
                        color: themeManager.textMuted; font.pixelSize: 10
                    }
                }
                MouseArea {
                    id: area; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.choose(parent.modelData)
                }
                Keys.onSpacePressed: root.choose(modelData)
                Keys.onReturnPressed: root.choose(modelData)
                Keys.onEnterPressed: root.choose(modelData)
            }
        }
    }
}

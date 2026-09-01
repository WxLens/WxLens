// SPDX-License-Identifier: MIT
import QtQuick

// Presentation-only browser. Product identity, availability and selection validation all live
// in PaneController; this surface only renders that model.
Rectangle {
    id: root
    required property var paneController
    signal closeRequested()

    width: Math.min(390, parent ? parent.width - 16 : 390)
    height: Math.min(460, parent ? parent.height - 16 : 460)
    radius: themeManager.cornerRadius
    color: themeManager.elevatedSurface
    border.color: themeManager.border
    border.width: 1

    // Consume wheel/touchpad input across the entire popup. A ListView at either boundary can
    // otherwise decline the event, allowing MapLibre's handler behind it to zoom the pane.
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            var delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y
                                                  : event.angleDelta.y / 2
            var minimum = productList.originY
            var maximum = Math.max(minimum,
                                   productList.originY + productList.contentHeight - productList.height)
            productList.contentY = Math.max(minimum,
                                            Math.min(maximum, productList.contentY - delta))
            event.accepted = true
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 7

        Row {
            width: parent.width
            spacing: 8
            Text { text: "Products"; color: themeManager.textPrimary; font.pixelSize: 16 }
            Text {
                text: root.paneController.sourceKey
                color: themeManager.textMuted
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: Math.max(0, parent.width - 150); height: 1 }
            Text {
                text: "×"; color: themeManager.textSecondary; font.pixelSize: 18
                MouseArea { anchors.fill: parent; anchors.margins: -7; onClicked: root.closeRequested() }
            }
        }

        Text {
            visible: root.paneController.productCatalogLoading
            text: "Discovering Level 3 products…"
            color: themeManager.textMuted
            font.pixelSize: 11
        }
        Text {
            visible: root.paneController.productCatalogError !== ""
            width: parent.width
            wrapMode: Text.Wrap
            text: root.paneController.productCatalogError
            color: themeManager.warning
            font.pixelSize: 11
        }

        ListView {
            id: productList
            width: parent.width
            height: parent.height - y - paletteRow.height - 8
            clip: true
            spacing: 3
            model: root.paneController.productCatalog

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 54
                radius: themeManager.cornerRadius
                color: selected ? themeManager.controlActive
                                : itemArea.containsMouse ? themeManager.controlHover
                                                         : themeManager.control
                border.color: selected ? themeManager.primary : themeManager.border
                readonly property bool selected:
                    root.paneController.productIdentity === modelData.identity

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 8
                    spacing: 2
                    Text {
                        text: modelData.description + (modelData.awipsId ? "  ·  " + modelData.awipsId : "")
                        color: themeManager.textPrimary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: modelData.category + "  ·  " +
                              (modelData.available ? "available" : "unavailable")
                        color: themeManager.textMuted
                        font.pixelSize: 10
                    }
                }
                MouseArea {
                    id: itemArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: parent.modelData.available
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.paneController.selectProduct(parent.modelData.identityKind,
                                                          parent.modelData.identity,
                                                          parent.modelData.description)
                        root.closeRequested()
                    }
                }
            }
        }

        Flow {
            id: paletteRow
            width: parent.width
            height: implicitHeight
            spacing: 5
            Text {
                text: "Palette"
                color: themeManager.textMuted
                font.pixelSize: 10
                height: 24
                verticalAlignment: Text.AlignVCenter
            }
            Repeater {
                model: ["Default"].concat(root.paneController.compatiblePaletteNames.filter(
                    function(name) { return name !== root.paneController.defaultPaletteName }))
                delegate: Rectangle {
                    required property string modelData
                    width: paletteText.implicitWidth + 12
                    height: 24
                    radius: themeManager.cornerRadius
                    readonly property bool selected:
                        (modelData === "Default" && root.paneController.paletteName === "") ||
                        root.paneController.paletteName === modelData
                    color: selected ? themeManager.controlActive : themeManager.control
                    border.color: selected ? themeManager.primary : themeManager.border
                    Text {
                        id: paletteText
                        anchors.centerIn: parent
                        text: parent.modelData === "Default"
                              ? "Default (" + root.paneController.defaultPaletteName + ")"
                              : parent.modelData
                        color: themeManager.textSecondary
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.paneController.paletteName =
                                   parent.modelData === "Default" ? "" : parent.modelData
                    }
                }
            }
        }
    }
}

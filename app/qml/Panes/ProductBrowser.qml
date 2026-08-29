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
            width: parent.width
            height: parent.height - y
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
    }
}

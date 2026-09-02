// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls

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
    property string query: ""
    property string pendingPaletteName: paneController ? paneController.paletteName : ""
    property var expandedCategories: ({})

    onVisibleChanged: if (visible && paneController) pendingPaletteName = paneController.paletteName
    readonly property var productGroups: {
        const needle = query.trim().toLowerCase()
        const products = paneController.productCatalog.filter(function(product) {
            return needle === "" || [product.description, product.category, product.awipsId,
                                      product.identity].join(" ").toLowerCase().indexOf(needle) >= 0
        })
        const groups = []
        const byName = ({})
        products.forEach(function(product) {
            const name = String(product.category)
            if (!byName[name]) {
                byName[name] = { category: name, products: [] }
                groups.push(byName[name])
            }
            byName[name].products.push(product)
        })
        return groups
    }

    function toggleCategory(category) {
        const next = Object.assign({}, expandedCategories)
        next[category] = !next[category]
        expandedCategories = next
    }

    function selectProduct(product) {
        paneController.selectProduct(product.identityKind, product.identity, product.description)
        closeRequested()
    }

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

        TextField {
            id: productSearch
            width: parent.width
            placeholderText: "Search name, category, or AWIPS ID"
            color: themeManager.textPrimary
            placeholderTextColor: themeManager.textMuted
            selectByMouse: true
            background: Rectangle {
                radius: themeManager.cornerRadius
                color: themeManager.control
                border.color: productSearch.activeFocus ? themeManager.primary : themeManager.border
            }
            onTextChanged: root.query = text
            Keys.onEscapePressed: root.closeRequested()
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
            model: root.productGroups

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                readonly property var selectedProduct: {
                    for (var i = 0; i < modelData.products.length; ++i) {
                        if (root.paneController.productIdentity === modelData.products[i].identity)
                            return modelData.products[i]
                    }
                    return modelData.products[0]
                }
                readonly property bool expanded:
                    root.query.trim() !== "" || root.expandedCategories[modelData.category] === true
                readonly property int alternativeCount: Math.max(0, modelData.products.length - 1)
                height: 58 + (expanded ? alternativeCount * 48 : 0)
                radius: themeManager.cornerRadius
                color: themeManager.control
                border.color: themeManager.border

                Rectangle {
                    id: familyRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 58
                    radius: themeManager.cornerRadius
                    color: root.paneController.productIdentity === parent.selectedProduct.identity
                           ? themeManager.controlActive
                           : familyArea.containsMouse ? themeManager.controlHover : "transparent"
                    Text {
                        anchors.left: parent.left; anchors.right: expandButton.left
                        anchors.top: parent.top; anchors.margins: 8
                        text: parent.parent.modelData.category
                        color: themeManager.textPrimary; font.pixelSize: 12; font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        anchors.left: parent.left; anchors.right: expandButton.left
                        anchors.bottom: parent.bottom; anchors.margins: 8
                        text: parent.parent.selectedProduct.description +
                              (parent.parent.selectedProduct.awipsId
                               ? "  ·  " + parent.parent.selectedProduct.awipsId : "")
                        color: themeManager.textMuted; font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        id: expandButton
                        visible: parent.parent.modelData.products.length > 1
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 7
                        width: 30; height: 30; radius: themeManager.cornerRadius
                        color: expandArea.containsMouse ? themeManager.controlHover : themeManager.elevatedSurface
                        Text {
                            anchors.centerIn: parent
                            text: familyRow.parent.expanded ? "⌃" : "⌄"
                            color: themeManager.textSecondary; font.pixelSize: 14
                        }
                        MouseArea {
                            id: expandArea; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.toggleCategory(familyRow.parent.modelData.category)
                        }
                    }
                    MouseArea {
                        id: familyArea
                        anchors.left: parent.left; anchors.right: expandButton.left
                        anchors.top: parent.top; anchors.bottom: parent.bottom
                        hoverEnabled: true
                        enabled: familyRow.parent.selectedProduct.available
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectProduct(familyRow.parent.selectedProduct)
                    }
                }

                Column {
                    visible: parent.expanded
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: familyRow.bottom
                    Repeater {
                        model: parent.parent.modelData.products.filter(function(product) {
                            return product.identity !== parent.parent.selectedProduct.identity
                        })
                        delegate: Rectangle {
                            required property var modelData
                            width: parent.width; height: 48
                            color: variantArea.containsMouse ? themeManager.controlHover : "transparent"
                            Text {
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter; anchors.margins: 14
                                text: modelData.description + (modelData.awipsId ? "  ·  " + modelData.awipsId : "")
                                color: themeManager.textSecondary; font.pixelSize: 11; elide: Text.ElideRight
                            }
                            MouseArea {
                                id: variantArea; anchors.fill: parent; hoverEnabled: true
                                enabled: parent.modelData.available; cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectProduct(parent.modelData)
                            }
                        }
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
                text: root.paneController.compatiblePaletteNames.length <= 1
                      ? "Palette · Default only (no alternatives installed)"
                      : "Palette"
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
                        (modelData === "Default" && root.pendingPaletteName === "") ||
                        root.pendingPaletteName === modelData
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
                        onClicked: root.pendingPaletteName =
                                   parent.modelData === "Default" ? "" : parent.modelData
                    }
                }
            }
            Rectangle {
                visible: root.paneController.compatiblePaletteNames.length > 1
                width: 54; height: 24; radius: themeManager.cornerRadius
                color: root.pendingPaletteName !== root.paneController.paletteName
                       ? themeManager.primary : themeManager.control
                Text { anchors.centerIn: parent; text: "Apply"; color: themeManager.textPrimary; font.pixelSize: 9 }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.pendingPaletteName !== root.paneController.paletteName
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.paneController.paletteName = root.pendingPaletteName
                }
            }
        }
    }
}

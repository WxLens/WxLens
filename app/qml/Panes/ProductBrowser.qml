// SPDX-License-Identifier: MIT
import QtQuick

import WxLens.App
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
    property var expandedExperts: ({})
    property var expandedVariants: ({})
    property int tiltLabelStyle: (typeof appSettings !== "undefined" && appSettings !== null)
        ? appSettings.productTiltLabelStyle : 0

    readonly property var productFamilies: {
        const needle = query.trim().toLowerCase()
        const products = paneController.productCatalog.filter(function(product) {
            return needle === "" || [product.description, product.family, product.category,
                                      product.elevationAngleText, product.awipsId,
                                      product.identity].join(" ").toLowerCase().indexOf(needle) >= 0
        })
        const sections = ({})
        const sectionOrder = ["Reflectivity Products", "Velocity Products",
                              "Dual-Polarization", "Precipitation Accumulation",
                              "Other Products"]
        products.forEach(function(product) {
            // The backend guarantees family for new catalogs. The fallback keeps an older or
            // partially populated catalog useful rather than creating a blank family row.
            const name = String(product.family || product.description || product.category)
            const section = String(product.category || "Other Products")
            if (!sections[section])
                sections[section] = { order: [], byName: ({}) }
            if (!sections[section].byName[name]) {
                sections[section].byName[name] = { family: name, section: section, products: [] }
                sections[section].order.push(name)
            }
            sections[section].byName[name].products.push(product)
        })
        const families = []
        const remainingSections = Object.keys(sections).filter(function(section) {
            return sectionOrder.indexOf(section) < 0
        })
        sectionOrder.concat(remainingSections).forEach(function(section) {
            if (!sections[section]) return
            sections[section].order.forEach(function(name, index) {
                const family = sections[section].byName[name]
                family.firstInSection = index === 0
                families.push(family)
            })
        })
        return families
    }

    function toggleExpert(family) {
        const next = Object.assign({}, expandedExperts)
        next[family] = !next[family]
        expandedExperts = next
    }

    function toggleVariants(family) {
        const next = Object.assign({}, expandedVariants)
        next[family] = !next[family]
        expandedVariants = next
    }

    function variantLabel(product) {
        const angle = String(product.elevationAngleText || "")
        const awips = String(product.awipsId || "")
        const fallback = String(product.description || product.identity || "Variant")
        if (tiltLabelStyle === 1)
            return angle || fallback
        if (tiltLabelStyle === 2)
            return awips || fallback
        if (angle && awips)
            return angle + "  ·  " + awips
        return angle || awips || fallback
    }

    function sourceLabel(product) {
        return product.identityKind === "level2" ? "Level 2 raw" : "Level 3"
    }

    function selectProduct(product) {
        paneController.selectProduct(product.identityKind, product.identity, product.description)
        closeRequested()
    }

    Connections {
        target: (typeof appSettings !== "undefined") ? appSettings : null
        function onProductTiltLabelStyleChanged() {
            root.tiltLabelStyle = appSettings.productTiltLabelStyle
        }
    }

    // Consume wheel/touchpad input across the entire popup. A ListView at either boundary can
    // otherwise decline the event, allowing MapLibre's handler behind it to zoom the pane.
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            var delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 2
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
            WxButton {
                width: 26; height: 26
                flat: true
                text: "×"
                font.pixelSize: 18
                name: "Close product browser"
                onClicked: root.closeRequested()
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
            placeholderText: "Search family, tilt, or AWIPS ID"
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
            height: parent.height - y
            clip: true
            spacing: 4
            model: root.productFamilies

            delegate: Rectangle {
                id: familyCard
                required property var modelData

                width: ListView.view.width
                height: familyContent.height + 16 + (modelData.firstInSection ? 28 : 0)
                radius: themeManager.cornerRadius
                color: themeManager.control
                border.color: themeManager.border

                readonly property var selectedProduct: {
                    var recommended = null
                    for (var i = 0; i < modelData.products.length; ++i) {
                        const product = modelData.products[i]
                        if (root.paneController.productIdentity === product.identity)
                            return product
                        if (recommended === null && product.recommended)
                            recommended = product
                    }
                    // Level 3 recommendations are intentionally not populated yet. Container
                    // order is only the neutral fallback until that backend work lands.
                    return recommended !== null ? recommended : modelData.products[0]
                }
                readonly property bool expertExpanded:
                    root.expandedExperts[modelData.family] === true
                readonly property bool variantsExpanded:
                    root.expandedVariants[modelData.family] === true || root.query.trim() !== ""

                Text {
                    visible: familyCard.modelData.firstInSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 4
                    text: familyCard.modelData.section
                    color: themeManager.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                }

                Column {
                    id: familyContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: familyCard.modelData.firstInSection ? 28 : 8
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Row {
                        width: parent.width
                        spacing: 6

                        Text {
                            width: parent.width - expertButton.width - 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: familyCard.modelData.family
                            color: themeManager.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        WxButton {
                            id: expertButton
                            width: 72
                            height: 24
                            flat: true
                            text: familyCard.expertExpanded ? "Less info" : "Details"
                            name: (familyCard.expertExpanded ? "Hide " : "Show ") +
                                  familyCard.modelData.family + " expert details"
                            onClicked: root.toggleExpert(familyCard.modelData.family)
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 5

                        WxButton {
                            width: parent.width - (variantButton.visible ? variantButton.width + 5 : 0)
                            text: root.variantLabel(familyCard.selectedProduct)
                            name: familyCard.modelData.family + ", " + text
                            highlighted: root.paneController.productIdentity ===
                                         familyCard.selectedProduct.identity
                            // Which product is displayed is the whole point of this row, and the
                            // highlight colour is the only other thing that says so.
                            selectable: true
                            enabled: familyCard.selectedProduct.available
                            height: 26
                            onClicked: root.selectProduct(familyCard.selectedProduct)
                        }
                        WxButton {
                            id: variantButton
                            visible: familyCard.modelData.products.length > 1
                            width: 74
                            height: 26
                            text: familyCard.variantsExpanded ? "Hide" : "Variants"
                            name: (familyCard.variantsExpanded ? "Hide " : "Show ") +
                                  familyCard.modelData.family + " variants"
                            onClicked: root.toggleVariants(familyCard.modelData.family)
                        }
                    }

                    Flow {
                        visible: familyCard.variantsExpanded &&
                                 familyCard.modelData.products.length > 1
                        width: parent.width
                        spacing: 5

                        Repeater {
                            model: familyCard.modelData.products
                            delegate: WxButton {
                                required property var modelData
                                text: root.variantLabel(modelData)
                                name: familyCard.modelData.family + ", " + text +
                                      (modelData.available ? "" : ", unavailable")
                                highlighted: familyCard.selectedProduct.identity === modelData.identity
                                selectable: true
                                enabled: modelData.available
                                height: 26
                                onClicked: root.selectProduct(modelData)
                            }
                        }
                    }

                    Rectangle {
                        visible: familyCard.expertExpanded
                        width: parent.width
                        height: visible ? expertDetails.height + 12 : 0
                        radius: themeManager.cornerRadius
                        color: themeManager.elevatedSurface
                        border.color: themeManager.border

                        Column {
                            id: expertDetails
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 6
                            spacing: 3

                            Text {
                                text: "Source: " + root.sourceLabel(familyCard.selectedProduct)
                                color: themeManager.textSecondary
                                font.pixelSize: 10
                            }
                            Text {
                                text: "AWIPS ID: " + (familyCard.selectedProduct.awipsId || "Not applicable")
                                color: themeManager.textSecondary
                                font.pixelSize: 10
                            }
                            Text {
                                text: "Available: " + (familyCard.selectedProduct.available ? "Yes" : "No")
                                color: familyCard.selectedProduct.available
                                       ? themeManager.textSecondary : themeManager.warning
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }
    }
}

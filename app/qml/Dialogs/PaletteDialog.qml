// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "../Controls"

Rectangle {
    id: root
    anchors.fill: parent
    visible: false
    z: 100
    color: "#99000000"
    property string notice: ""

    // Which product family the editor is filtered to. Empty means "all palettes", which is only
    // useful for browsing - working on one field is the normal case, and an unfiltered strip of
    // every bundled ramp plus every import is what made this hard to use.
    property string familyFilter: ""
    property var importPreview: null

    readonly property var visiblePalettes: paletteManager.palettesForFamily(familyFilter)

    function familyLabel(familyId) {
        const list = paletteManager.families
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === familyId) return list[i].label
        return familyId
    }

    function paletteDescription(name) {
        const names = {
            "DR": "Reflectivity", "DV": "Base velocity",
            "SRV": "Storm-relative velocity", "SW": "Spectrum width",
            "ZDR": "Differential reflectivity", "CC": "Correlation coefficient",
            "KDP": "Specific differential phase",
            "KDP2": "Alternate specific differential phase",
            "HC": "Hydrometeor classification", "ET": "Echo tops",
            "VIL": "Vertically integrated liquid", "OHP": "One-hour precipitation",
            "STP": "Storm-total precipitation", "DOD_DSD": "Drop-size distribution",
            "Default16": "Generic 16-color palette"
        }
        return names[name] || name
    }
    function showNotice(message) {
        notice = message
        noticeTimer.restart()
    }
    function open() { visible = true }
    function close() { paletteManager.requestClose() }

    Connections {
        target: paletteManager
        function onCloseRequested() { root.visible = false; root.importPreview = null }
        function onImportFileRequested() { openDialog.open() }
        function onSaveFileRequested() { saveDialog.pending = true; saveDialog.open() }
    }

    // Dropping a .pal anywhere on the dialog opens the same preview the Import button does, so a
    // file manager drag is a first-class way in rather than a second implementation.
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (!drop.hasUrls || drop.urls.length === 0) return
            root.importPreview = paletteManager.inspectFile(drop.urls[0])
            root.importPreview.source = drop.urls[0]
            drop.accept()
        }
    }
    MouseArea {
        anchors.fill: parent
        preventStealing: true
        onClicked: root.close()
        onWheel: (wheel) => wheel.accepted = true
    }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(760, root.width - 40); height: Math.min(650, root.height - 40)
        radius: themeManager.cornerRadius; color: themeManager.surface; border.color: themeManager.border
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onWheel: (wheel) => wheel.accepted = true
        }
        Column {
            anchors.fill: parent; anchors.margins: 18; spacing: 12
            Row {
                width: parent.width
                Text { text: "Radar palette"; color: themeManager.textPrimary; font.pixelSize: 18; font.bold: true }
                Item { width: parent.width - 300; height: 1 }
                Text {
                    text: "Import .pal"; color: themeManager.accent; font.pixelSize: 12
                    MouseArea { anchors.fill: parent; preventStealing: true; onClicked: paletteManager.requestImport() }
                    ToolTip.visible: importHover.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: "Browse for a .pal, or drag one onto this dialog"
                    MouseArea { id: importHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                }
                Item { width: 18; height: 1 }
                Text { text: "Save as"; color: themeManager.accent; font.pixelSize: 12; MouseArea { anchors.fill: parent; preventStealing: true; onClicked: { saveDialog.pending = false; saveDialog.open() } } }
            }
            Text {
                text: paletteManager.activeName + (paletteManager.editor.dirty ? " - modified" : "") + (paletteManager.editor.valid ? "" : " - invalid")
                color: paletteManager.editor.valid ? themeManager.textSecondary : themeManager.danger; font.pixelSize: 12
            }
            Text {
                text: "Choose and edit a palette here. Changes remain a draft until you press Apply."
                color: themeManager.textMuted; font.pixelSize: 10
            }

            // Field filter. Picking a field narrows the strip below to that field's ramps plus any
            // import that can colour it, which is the "when I'm working on velocity, only show me
            // what works with velocity" case.
            Row {
                width: parent.width; spacing: 8
                Text {
                    text: "Field"; color: themeManager.textSecondary; font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: 210; height: 26; radius: themeManager.cornerRadius
                    color: themeManager.control; border.color: themeManager.border
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 8
                        anchors.right: parent.right; anchors.rightMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.familyFilter === "" ? "All palettes" : root.familyLabel(root.familyFilter)
                        color: themeManager.textPrimary; font.pixelSize: 10; elide: Text.ElideRight
                    }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 7
                        anchors.verticalCenter: parent.verticalCenter
                        text: "⌄"; color: themeManager.textSecondary; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; preventStealing: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fieldMenu.visible = !fieldMenu.visible
                    }
                }
                Text {
                    visible: root.familyFilter !== ""
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        const list = paletteManager.families
                        for (var i = 0; i < list.length; ++i)
                            if (list[i].id === root.familyFilter)
                                return list[i].usesBundledDefault
                                    ? "using the bundled " + list[i].id
                                    : "using " + list[i].defaultName
                        return ""
                    }
                    color: themeManager.textMuted; font.pixelSize: 10
                }
                Text {
                    visible: root.familyFilter !== "" && !paletteManager.families.some(function(f) {
                        return f.id === root.familyFilter && f.usesBundledDefault })
                    anchors.verticalCenter: parent.verticalCenter
                    text: "restore bundled"
                    color: themeManager.accent; font.pixelSize: 10
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -6; preventStealing: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            paletteManager.setFamilyDefault(root.familyFilter, "")
                            root.showNotice(root.familyLabel(root.familyFilter) +
                                            " is back to its bundled palette")
                        }
                    }
                }
            }

            Flickable {
                width: parent.width; height: 30; contentWidth: choices.width; clip: true
                Row { id: choices; spacing: 6
                    Repeater { model: root.visiblePalettes
                        Rectangle {
                            id: paletteChoice
                            required property string modelData
                            // Which palette each product family currently renders with. Marked on
                            // the chip so "which one is my velocity default right now" is visible
                            // without applying anything.
                            readonly property bool familyDefault:
                                paletteManager.familyDefaultNames.indexOf(modelData) >= 0
                            width: label.implicitWidth + 16 + (familyDefault ? 10 : 0); height: 26; radius: 4
                            color: modelData === paletteManager.activeName ? themeManager.controlActive : themeManager.control
                            border.width: familyDefault ? 1 : 0
                            border.color: themeManager.primary
                            Row {
                                anchors.centerIn: parent; spacing: 4
                                Text { id: label; text: paletteChoice.modelData; color: themeManager.textPrimary; font.pixelSize: 10 }
                                Text { visible: paletteChoice.familyDefault; text: "●"; color: themeManager.primary; font.pixelSize: 7; anchors.verticalCenter: parent.verticalCenter }
                            }
                            MouseArea {
                                id: paletteChoiceArea
                                anchors.fill: parent; hoverEnabled: true
                                preventStealing: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: paletteManager.requestSelect(modelData)
                            }
                            ToolTip.visible: paletteChoiceArea.containsMouse
                            ToolTip.delay: 450
                            ToolTip.text: {
                                const family = paletteManager.familyOf(modelData)
                                const units = paletteManager.unitsOf(modelData)
                                var text = root.paletteDescription(modelData)
                                if (units !== "") text += " · " + units
                                if (family === "")
                                    text += " — imported, not linked to a field yet"
                                else if (familyDefault)
                                    text += " — current default for " + root.familyLabel(family).toLowerCase()
                                return text + " — click to edit; does not apply to a pane"
                            }
                        }
                    }
                }
            }

            Text {
                visible: root.visiblePalettes.length === 0
                width: parent.width; wrapMode: Text.Wrap
                text: "No palette can colour this field yet. Import a .pal whose Units match, or " +
                      "switch the field filter to All palettes."
                color: themeManager.textMuted; font.pixelSize: 10
            }
            Canvas {
                id: preview
                width: parent.width; height: 44
                onPaint: {
                    var ctx = getContext("2d"), stops = paletteManager.editor.previewStops
                    ctx.clearRect(0, 0, width, height)
                    for (var i = 0; i < stops.length; ++i) { ctx.fillStyle = stops[i].color; ctx.fillRect(i * width / stops.length, 0, Math.ceil(width / stops.length) + 1, height) }
                }
                Connections { target: paletteManager.editor; function onPreviewChanged() { preview.requestPaint() } }
                Repeater { model: paletteManager.editor
                    Rectangle {
                        required property real value; required property color stopColor; required property int index
                        width: 12; height: 18; y: 30; radius: 3; color: stopColor; border.color: "white"
                        x: (value - paletteManager.editor.minimumValue) / Math.max(0.0001, paletteManager.editor.maximumValue - paletteManager.editor.minimumValue) * (preview.width - width)
                        // A single-stop palette has minimumValue === maximumValue, so there is no
                        // position along the strip that means anything - dragging can't set a
                        // useful value, so don't offer the drag at all rather than silently
                        // snapping back to minimumValue on release.
                        MouseArea { anchors.fill: parent; preventStealing: true; drag.target: paletteManager.editor.maximumValue > paletteManager.editor.minimumValue ? parent : undefined; drag.axis: Drag.XAxis; drag.minimumX: 0; drag.maximumX: preview.width - parent.width
                            onPressed: stopList.currentIndex = index
                            onReleased: if (paletteManager.editor.maximumValue > paletteManager.editor.minimumValue) paletteManager.editor.setStopValue(index, paletteManager.editor.minimumValue + parent.x / (preview.width - parent.width) * (paletteManager.editor.maximumValue - paletteManager.editor.minimumValue)) }
                    }
                }
            }
            Row {
                width: parent.width; height: parent.height - 187; spacing: 14
                ListView {
                    id: stopList
                    width: parent.width - 250; height: parent.height; clip: true; model: paletteManager.editor
                    onCurrentIndexChanged: Qt.callLater(hexInput.sync)
                    delegate: Rectangle {
                        required property real value; required property color stopColor; required property color secondColor; required property bool hasSecondColor; required property int index
                        width: stopList.width; height: 34; color: ListView.isCurrentItem ? themeManager.controlActive : (index % 2 ? themeManager.elevatedSurface : "transparent")
                        Row { anchors.verticalCenter: parent.verticalCenter; spacing: 8
                            Rectangle { width: 22; height: 22; radius: 3; color: stopColor; border.color: themeManager.border }
                            Rectangle { visible: hasSecondColor; width: 22; height: 22; radius: 3; color: secondColor; border.color: themeManager.border }
                            TextInput { width: 85; text: value.toFixed(2); color: themeManager.textPrimary; selectByMouse: true; onEditingFinished: paletteManager.editor.setStopValue(index, Number(text)) }
                            Text { text: String(stopColor); color: themeManager.textMuted; font.pixelSize: 11 }
                        }
                        MouseArea { anchors.fill: parent; preventStealing: true; onClicked: stopList.currentIndex = index; z: -1 }
                    }
                }
                Column {
                    id: editPanel
                    property bool second: false
                    spacing: 8
                    Row { spacing: 8
                        Rectangle { width: 104; height: 25; radius: themeManager.cornerRadius; color: !editPanel.second ? themeManager.controlActive : themeManager.control
                            Text { anchors.centerIn: parent; text: "First color"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; preventStealing: true; onClicked: { editPanel.second = false; hexInput.sync() } } }
                        Rectangle { visible: stopList.currentItem && stopList.currentItem.hasSecondColor; width: 104; height: 25; radius: themeManager.cornerRadius; color: editPanel.second ? themeManager.controlActive : themeManager.control
                            Text { anchors.centerIn: parent; text: "Second color"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; preventStealing: true; onClicked: { editPanel.second = true; hexInput.sync() } } }
                    }
                    ColorPicker { visible: stopList.currentIndex >= 0; colorValue: !stopList.currentItem ? "white" : editPanel.second ? stopList.currentItem.secondColor : stopList.currentItem.stopColor; onColorEdited: (value) => paletteManager.editor.setStopColor(stopList.currentIndex, value, editPanel.second) }
                    Text { text: "Hex color (#RRGGBB or #AARRGGBB)"; color: themeManager.textSecondary; font.pixelSize: 10 }
                    Rectangle {
                        width: 220; height: 30; radius: themeManager.cornerRadius; color: themeManager.control; border.color: hexInput.acceptableInput ? themeManager.border : themeManager.danger
                        TextInput {
                            id: hexInput
                            anchors.fill: parent; anchors.margins: 7; text: "#ffffff"; color: themeManager.textPrimary; selectByMouse: true; font.family: "monospace"
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/ }
                            function sync() { if (!activeFocus && stopList.currentItem) text = String(editPanel.second ? stopList.currentItem.secondColor : stopList.currentItem.stopColor) }
                            onEditingFinished: if (acceptableInput && stopList.currentIndex >= 0) paletteManager.editor.setStopColor(stopList.currentIndex, text, editPanel.second)
                            Connections { target: paletteManager.editor; function onDataChanged() { hexInput.sync() } }
                        }
                    }
                    Rectangle {
                        width: 216; height: 30; radius: themeManager.cornerRadius; color: themeManager.primary
                            Text { anchors.centerIn: parent; text: "Apply to product"; color: "white"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; preventStealing: true; onClicked: {
                                const name = paletteManager.activeName
                                const family = paletteManager.familyOf(name)
                                paletteManager.applyActive()
                                root.showNotice(family === ""
                                    ? name + " applied — imported palettes stay editor-only until they declare a product family"
                                    : name + " applied — now the default for "
                                      + root.paletteDescription(family).toLowerCase()
                                      + " products (panes with their own palette keep it)")
                            } } }
                    Row { spacing: 8
                        Rectangle { width: 104; height: 28; radius: themeManager.cornerRadius; color: themeManager.control
                            Text { anchors.centerIn: parent; text: "Reset " + paletteManager.activeName; color: paletteManager.activeIsFactoryPalette ? themeManager.textPrimary : themeManager.textMuted; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; preventStealing: true; enabled: paletteManager.activeIsFactoryPalette; onClicked: {
                                if (!paletteManager.editor.dirty) {
                                    root.showNotice(paletteManager.activeName + " is already the original palette")
                                } else {
                                    paletteManager.requestResetActive()
                                    root.showNotice(paletteManager.activeName + " restored to its original palette")
                                }
                            } } }
                        Rectangle { width: 104; height: 28; radius: themeManager.cornerRadius; color: themeManager.control
                            Text { anchors.centerIn: parent; text: "Reset all"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; preventStealing: true; onClicked: {
                                paletteManager.requestResetAll()
                                root.showNotice("Factory palettes and product defaults restored")
                            } } }
                    }
                }
            }
        }
    }
    Rectangle {
        visible: root.notice !== ""
        z: 300
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        width: Math.min(noticeText.implicitWidth + 28, parent.width - 40)
        height: 38
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.primary
        Text {
            id: noticeText
            anchors.centerIn: parent
            text: root.notice
            color: themeManager.textPrimary
            font.pixelSize: 11
        }
    }
    Timer { id: noticeTimer; interval: 2800; onTriggered: root.notice = "" }

    // Field filter menu. A plain list rather than a ComboBox so it inherits the theme tokens the
    // rest of this dialog uses.
    Rectangle {
        id: fieldMenu
        visible: false
        z: 210
        anchors.centerIn: parent
        width: 330
        height: Math.min(root.height - 80, fieldColumn.implicitHeight + 16)
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        MouseArea { anchors.fill: parent; preventStealing: true; onWheel: (wheel) => wheel.accepted = true }
        Flickable {
            anchors.fill: parent; anchors.margins: 8
            contentHeight: fieldColumn.implicitHeight; clip: true
            Column {
                id: fieldColumn
                width: parent.width; spacing: 2
                Rectangle {
                    width: parent.width; height: 30; radius: themeManager.cornerRadius
                    color: root.familyFilter === "" ? themeManager.controlActive : "transparent"
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "All palettes"; color: themeManager.textPrimary; font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent; preventStealing: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { root.familyFilter = ""; fieldMenu.visible = false }
                    }
                }
                Repeater {
                    model: paletteManager.families
                    delegate: Rectangle {
                        required property var modelData
                        width: fieldColumn.width; height: 34; radius: themeManager.cornerRadius
                        color: root.familyFilter === modelData.id ? themeManager.controlActive
                                                                  : fieldArea.containsMouse ? themeManager.controlHover
                                                                                            : "transparent"
                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 8
                            anchors.top: parent.top; anchors.topMargin: 4
                            text: modelData.label; color: themeManager.textPrimary; font.pixelSize: 11
                        }
                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 8
                            anchors.bottom: parent.bottom; anchors.bottomMargin: 4
                            text: (modelData.units === "" ? "no declared units" : modelData.units) +
                                  " · " + modelData.defaultName +
                                  (modelData.usesBundledDefault ? " (bundled)" : "")
                            color: themeManager.textMuted; font.pixelSize: 9
                        }
                        MouseArea {
                            id: fieldArea
                            anchors.fill: parent; hoverEnabled: true; preventStealing: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { root.familyFilter = modelData.id; fieldMenu.visible = false }
                        }
                    }
                }
            }
        }
    }

    // Import preview: what the file is, what it can colour, and the one click that links it.
    MouseArea {
        anchors.fill: parent; preventStealing: true; visible: root.importPreview !== null; z: 219
        onWheel: (wheel) => wheel.accepted = true
    }
    Rectangle {
        visible: root.importPreview !== null
        z: 220
        anchors.centerIn: parent
        width: 470
        height: importColumn.implicitHeight + 36
        radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface
        border.color: themeManager.border
        MouseArea { anchors.fill: parent; preventStealing: true }
        Column {
            id: importColumn
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 18; spacing: 10
            Text {
                text: "Import palette"; color: themeManager.textPrimary
                font.pixelSize: 16; font.bold: true
            }
            Text {
                width: parent.width; wrapMode: Text.Wrap
                text: root.importPreview ? (root.importPreview.name || "(unnamed)") : ""
                color: themeManager.textPrimary; font.pixelSize: 12
            }
            Text {
                visible: root.importPreview && !root.importPreview.valid
                width: parent.width; wrapMode: Text.Wrap
                text: root.importPreview ? "⚠ " + root.importPreview.error : ""
                color: themeManager.warning; font.pixelSize: 11
            }
            Text {
                visible: root.importPreview && root.importPreview.valid
                width: parent.width; wrapMode: Text.Wrap
                text: {
                    if (!root.importPreview || !root.importPreview.valid) return ""
                    const units = root.importPreview.units
                    return root.importPreview.stopCount + " colour stops · units " +
                           (units === "" ? "not declared" : units)
                }
                color: themeManager.textSecondary; font.pixelSize: 11
            }
            Text {
                visible: root.importPreview && root.importPreview.valid
                width: parent.width; wrapMode: Text.Wrap
                text: {
                    if (!root.importPreview || !root.importPreview.valid) return ""
                    const list = root.importPreview.compatibleFamilies
                    if (!list || list.length === 0)
                        return "No field matches these units, so WxLens cannot tell what this " +
                               "palette measures. Import it to edit, then link it from the field " +
                               "filter once you know."
                    var names = []
                    for (var i = 0; i < list.length; ++i) names.push(list[i].label.toLowerCase())
                    return "Works with: " + names.join(", ") +
                           ". Pick one to use it there, or import it unlinked."
                }
                color: themeManager.textMuted; font.pixelSize: 10
            }
            Flow {
                visible: root.importPreview && root.importPreview.valid
                width: parent.width; spacing: 6
                Repeater {
                    model: root.importPreview && root.importPreview.valid
                           ? root.importPreview.compatibleFamilies : []
                    delegate: Rectangle {
                        required property var modelData
                        width: familyText.implicitWidth + 20; height: 28
                        radius: themeManager.cornerRadius
                        color: linkArea.containsMouse ? themeManager.controlHover : themeManager.primary
                        Text {
                            id: familyText; anchors.centerIn: parent
                            text: "Use for " + modelData.label.toLowerCase()
                            color: "white"; font.pixelSize: 10
                        }
                        MouseArea {
                            id: linkArea
                            anchors.fill: parent; hoverEnabled: true; preventStealing: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const source = root.importPreview.source
                                const familyId = modelData.id
                                root.importPreview = null
                                if (!paletteManager.openFile(source, familyId)) {
                                    root.showNotice("Could not import that palette")
                                    return
                                }
                                root.familyFilter = familyId
                                root.showNotice(paletteManager.activeName + " imported and linked to " +
                                                modelData.label.toLowerCase() +
                                                " — press Apply to product to use it")
                            }
                        }
                    }
                }
            }
            Row {
                spacing: 8
                Rectangle {
                    visible: root.importPreview && root.importPreview.valid
                    width: 132; height: 28; radius: themeManager.cornerRadius
                    color: themeManager.control; border.color: themeManager.border
                    Text {
                        anchors.centerIn: parent; text: "Import unlinked"
                        color: themeManager.textPrimary; font.pixelSize: 10
                    }
                    MouseArea {
                        anchors.fill: parent; preventStealing: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const source = root.importPreview.source
                            root.importPreview = null
                            if (paletteManager.openFile(source, ""))
                                root.showNotice(paletteManager.activeName +
                                                " imported — link it to a field to use it on a pane")
                            else
                                root.showNotice("Could not import that palette")
                        }
                    }
                }
                Rectangle {
                    width: 90; height: 28; radius: themeManager.cornerRadius
                    color: themeManager.control; border.color: themeManager.border
                    Text {
                        anchors.centerIn: parent; text: "Cancel"
                        color: themeManager.textPrimary; font.pixelSize: 10
                    }
                    MouseArea {
                        anchors.fill: parent; preventStealing: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.importPreview = null
                    }
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; preventStealing: true; visible: paletteManager.confirmationRequired; z: 199 }
    // Two genuinely different situations, so two different warnings. Applied edits are live on
    // panes but exist only in memory - a restart brings the bundled palette back - whereas an
    // unapplied draft has not coloured anything yet. Saying "save changes?" for both told the user
    // their applied change might be lost *and* that it might not have happened.
    Rectangle {
        visible: paletteManager.confirmationRequired; z: 200; anchors.centerIn: parent
        width: 430; height: confirmColumn.implicitHeight + 36; radius: themeManager.cornerRadius
        color: themeManager.elevatedSurface; border.color: themeManager.border
        Column {
            id: confirmColumn
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 18; spacing: 13
            Text {
                text: paletteManager.activeDraftApplied
                      ? paletteManager.activeName + " is applied but not saved to a file"
                      : "Discard unapplied changes to " + paletteManager.activeName + "?"
                width: parent.width; wrapMode: Text.WordWrap
                color: themeManager.textPrimary; font.pixelSize: 16; font.bold: true
            }
            Text {
                text: paletteManager.activeDraftApplied
                      ? "Panes are using it now, but factory palettes are never overwritten, so " +
                        "this edit is gone when WxLens restarts unless you save a .pal copy."
                      : "These changes have not coloured any pane yet. Save a separate .pal copy, " +
                        "discard them, or keep editing."
                width: parent.width; wrapMode: Text.WordWrap
                color: themeManager.textSecondary; font.pixelSize: 11
            }
            Row { spacing: 10
                Rectangle { width: 104; height: 30; radius: themeManager.cornerRadius; color: themeManager.primary
                    Text { anchors.centerIn: parent; text: "Save a copy"; color: "white" }
                    MouseArea { anchors.fill: parent; preventStealing: true; onClicked: paletteManager.resolveUnsavedChanges(0) } }
                Rectangle { width: 110; height: 30; radius: themeManager.cornerRadius
                    color: paletteManager.activeDraftApplied ? themeManager.control : themeManager.danger
                    Text {
                        anchors.centerIn: parent
                        text: paletteManager.activeDraftApplied ? "Close anyway" : "Discard"
                        color: themeManager.textPrimary
                    }
                    MouseArea { anchors.fill: parent; preventStealing: true; onClicked: paletteManager.resolveUnsavedChanges(1) } }
                Rectangle { width: 100; height: 30; radius: themeManager.cornerRadius; color: themeManager.control
                    Text { anchors.centerIn: parent; text: "Keep editing"; color: themeManager.textPrimary }
                    MouseArea { anchors.fill: parent; preventStealing: true; onClicked: paletteManager.resolveUnsavedChanges(2) } }
            }
        }
    }
    FileDialog {
        id: openDialog
        title: "Open GRLevelX palette"
        nameFilters: ["Palette files (*.pal)"]
        // Inspect first, import second: the preview is where the user learns what the file can
        // colour and links it, so browsing and dropping a file both land in the same place.
        onAccepted: {
            root.importPreview = paletteManager.inspectFile(selectedFile)
            root.importPreview.source = selectedFile
        }
    }
    FileDialog { id: saveDialog; property bool pending: false; title: "Save palette as"; fileMode: FileDialog.SaveFile; nameFilters: ["Palette files (*.pal)"]; onAccepted: pending ? paletteManager.completePendingSave(selectedFile) : paletteManager.saveAs(selectedFile) }
}

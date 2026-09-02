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
        function onCloseRequested() { root.visible = false }
        function onImportFileRequested() { openDialog.open() }
        function onSaveFileRequested() { saveDialog.pending = true; saveDialog.open() }
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
                Text { text: "Import .pal"; color: themeManager.accent; font.pixelSize: 12; MouseArea { anchors.fill: parent; onClicked: paletteManager.requestImport() } }
                Item { width: 18; height: 1 }
                Text { text: "Save as"; color: themeManager.accent; font.pixelSize: 12; MouseArea { anchors.fill: parent; onClicked: { saveDialog.pending = false; saveDialog.open() } } }
            }
            Text {
                text: paletteManager.activeName + (paletteManager.editor.dirty ? " - modified" : "") + (paletteManager.editor.valid ? "" : " - invalid")
                color: paletteManager.editor.valid ? themeManager.textSecondary : themeManager.danger; font.pixelSize: 12
            }
            Text {
                text: "Choose and edit a palette here. Changes remain a draft until you press Apply."
                color: themeManager.textMuted; font.pixelSize: 10
            }
            Flickable {
                width: parent.width; height: 30; contentWidth: choices.width; clip: true
                Row { id: choices; spacing: 6
                    Repeater { model: paletteManager.paletteNames
                        Rectangle {
                            id: paletteChoice
                            required property string modelData
                            width: label.implicitWidth + 16; height: 26; radius: 4
                            color: modelData === paletteManager.activeName ? themeManager.controlActive : themeManager.control
                            Text { id: label; anchors.centerIn: parent; text: modelData; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea {
                                id: paletteChoiceArea
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: paletteManager.requestSelect(modelData)
                            }
                            ToolTip.visible: paletteChoiceArea.containsMouse
                            ToolTip.delay: 450
                            ToolTip.text: root.paletteDescription(modelData) +
                                          " — click to edit; does not apply to a pane"
                        }
                    }
                }
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
                        MouseArea { anchors.fill: parent; onClicked: stopList.currentIndex = index; z: -1 }
                    }
                }
                Column {
                    id: editPanel
                    property bool second: false
                    spacing: 8
                    Row { spacing: 8
                        Rectangle { width: 104; height: 25; radius: themeManager.cornerRadius; color: !editPanel.second ? themeManager.controlActive : themeManager.control
                            Text { anchors.centerIn: parent; text: "First color"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: { editPanel.second = false; hexInput.sync() } } }
                        Rectangle { visible: stopList.currentItem && stopList.currentItem.hasSecondColor; width: 104; height: 25; radius: themeManager.cornerRadius; color: editPanel.second ? themeManager.controlActive : themeManager.control
                            Text { anchors.centerIn: parent; text: "Second color"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: { editPanel.second = true; hexInput.sync() } } }
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
                    Row { spacing: 8
                        Rectangle { width: 104; height: 28; radius: themeManager.cornerRadius; color: themeManager.primary
                            Text { anchors.centerIn: parent; text: "Apply to product"; color: "white"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: {
                                paletteManager.applyActive()
                                root.showNotice(paletteManager.activeName + " applied to all compatible panes")
                            } } }
                        Rectangle { width: 104; height: 28; radius: themeManager.cornerRadius; color: themeManager.control
                            Text { anchors.centerIn: parent; text: "Reset " + paletteManager.activeName; color: paletteManager.activeIsFactoryPalette ? themeManager.textPrimary : themeManager.textMuted; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; enabled: paletteManager.activeIsFactoryPalette; onClicked: {
                                if (!paletteManager.editor.dirty) {
                                    root.showNotice(paletteManager.activeName + " is already the original palette")
                                } else {
                                    paletteManager.requestResetActive()
                                    root.showNotice(paletteManager.activeName + " restored to its original palette")
                                }
                            } } }
                        Rectangle { width: 104; height: 28; radius: themeManager.cornerRadius; color: themeManager.control
                            Text { anchors.centerIn: parent; text: "Reset all"; color: themeManager.textPrimary; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: {
                                paletteManager.requestResetAll()
                                root.showNotice("Factory palettes restored; unchanged palettes were already original")
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
    MouseArea { anchors.fill: parent; visible: paletteManager.confirmationRequired; z: 199 }
    Rectangle {
        visible: paletteManager.confirmationRequired; z: 200; anchors.centerIn: parent; width: 410; height: 155; radius: themeManager.cornerRadius; color: themeManager.elevatedSurface; border.color: themeManager.border
        Column { anchors.fill: parent; anchors.margins: 18; spacing: 13
            Text { text: "Save changes to " + paletteManager.activeName + "?"; color: themeManager.textPrimary; font.pixelSize: 16; font.bold: true }
            Text { text: "Factory palettes are never overwritten. Save a separate .pal copy, discard the working changes, or keep editing."; width: parent.width; wrapMode: Text.WordWrap; color: themeManager.textSecondary; font.pixelSize: 11 }
            Row { spacing: 10
                Rectangle { width: 104; height: 30; radius: themeManager.cornerRadius; color: themeManager.primary
                    Text { anchors.centerIn: parent; text: "Save a copy"; color: "white" }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(0) } }
                Rectangle { width: 90; height: 30; radius: themeManager.cornerRadius; color: themeManager.danger
                    Text { anchors.centerIn: parent; text: "Discard"; color: themeManager.textPrimary }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(1) } }
                Rectangle { width: 100; height: 30; radius: themeManager.cornerRadius; color: themeManager.control
                    Text { anchors.centerIn: parent; text: "Keep editing"; color: themeManager.textPrimary }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(2) } }
            }
        }
    }
    FileDialog { id: openDialog; title: "Open GRLevelX palette"; nameFilters: ["Palette files (*.pal)"]; onAccepted: paletteManager.openFile(selectedFile) }
    FileDialog { id: saveDialog; property bool pending: false; title: "Save palette as"; fileMode: FileDialog.SaveFile; nameFilters: ["Palette files (*.pal)"]; onAccepted: pending ? paletteManager.completePendingSave(selectedFile) : paletteManager.saveAs(selectedFile) }
}

// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Dialogs
import "../Controls"

Rectangle {
    id: root
    anchors.fill: parent
    visible: false
    z: 100
    color: "#99000000"
    function open() { visible = true }
    function close() { paletteManager.requestClose() }

    Connections {
        target: paletteManager
        function onCloseRequested() { root.visible = false }
        function onImportFileRequested() { openDialog.open() }
        function onSaveFileRequested() { saveDialog.pending = true; saveDialog.open() }
    }
    MouseArea { anchors.fill: parent; onClicked: root.close() }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(760, root.width - 40); height: Math.min(650, root.height - 40)
        radius: 10; color: "#171d24"; border.color: "#39434e"
        MouseArea { anchors.fill: parent }
        Column {
            anchors.fill: parent; anchors.margins: 18; spacing: 12
            Row {
                width: parent.width
                Text { text: "Radar palette"; color: "#edf2f7"; font.pixelSize: 18; font.bold: true }
                Item { width: parent.width - 300; height: 1 }
                Text { text: "Import .pal"; color: "#73aee8"; font.pixelSize: 12; MouseArea { anchors.fill: parent; onClicked: paletteManager.requestImport() } }
                Item { width: 18; height: 1 }
                Text { text: "Save as"; color: "#73aee8"; font.pixelSize: 12; MouseArea { anchors.fill: parent; onClicked: { saveDialog.pending = false; saveDialog.open() } } }
            }
            Text {
                text: paletteManager.activeName + (paletteManager.editor.dirty ? " - modified" : "") + (paletteManager.editor.valid ? "" : " - invalid")
                color: paletteManager.editor.valid ? "#aab6c3" : "#ef7c7c"; font.pixelSize: 12
            }
            Flickable {
                width: parent.width; height: 30; contentWidth: choices.width; clip: true
                Row { id: choices; spacing: 6
                    Repeater { model: paletteManager.paletteNames
                        Rectangle {
                            required property string modelData
                            width: label.implicitWidth + 16; height: 26; radius: 4
                            color: modelData === paletteManager.activeName ? "#31557a" : "#222b34"
                            Text { id: label; anchors.centerIn: parent; text: modelData; color: "#dce5ee"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: paletteManager.requestSelect(modelData) }
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
                        MouseArea { anchors.fill: parent; drag.target: parent; drag.axis: Drag.XAxis; drag.minimumX: 0; drag.maximumX: preview.width - parent.width
                            onPressed: stopList.currentIndex = index
                            onReleased: paletteManager.editor.setStopValue(index, paletteManager.editor.minimumValue + parent.x / (preview.width - parent.width) * (paletteManager.editor.maximumValue - paletteManager.editor.minimumValue)) }
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
                        width: stopList.width; height: 34; color: ListView.isCurrentItem ? "#273442" : (index % 2 ? "#1b222a" : "transparent")
                        Row { anchors.verticalCenter: parent.verticalCenter; spacing: 8
                            Rectangle { width: 22; height: 22; radius: 3; color: stopColor; border.color: "#647383" }
                            Rectangle { visible: hasSecondColor; width: 22; height: 22; radius: 3; color: secondColor; border.color: "#647383" }
                            TextInput { width: 85; text: value.toFixed(2); color: "#e2e8ee"; selectByMouse: true; onEditingFinished: paletteManager.editor.setStopValue(index, Number(text)) }
                            Text { text: String(stopColor); color: "#8f9dab"; font.pixelSize: 11 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: stopList.currentIndex = index; z: -1 }
                    }
                }
                Column {
                    id: editPanel
                    property bool second: false
                    spacing: 8
                    Row { spacing: 8
                        Rectangle { width: 104; height: 25; radius: 4; color: !editPanel.second ? "#31557a" : "#293541"
                            Text { anchors.centerIn: parent; text: "First color"; color: "white"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: { editPanel.second = false; hexInput.sync() } } }
                        Rectangle { visible: stopList.currentItem && stopList.currentItem.hasSecondColor; width: 104; height: 25; radius: 4; color: editPanel.second ? "#31557a" : "#293541"
                            Text { anchors.centerIn: parent; text: "Second color"; color: "white"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: { editPanel.second = true; hexInput.sync() } } }
                    }
                    ColorPicker { visible: stopList.currentIndex >= 0; colorValue: !stopList.currentItem ? "white" : editPanel.second ? stopList.currentItem.secondColor : stopList.currentItem.stopColor; onColorEdited: (value) => paletteManager.editor.setStopColor(stopList.currentIndex, value, editPanel.second) }
                    Text { text: "Hex color (#RRGGBB or #AARRGGBB)"; color: "#9eabb8"; font.pixelSize: 10 }
                    Rectangle {
                        width: 220; height: 30; radius: 4; color: "#11171d"; border.color: hexInput.acceptableInput ? "#3a4652" : "#b75a5a"
                        TextInput {
                            id: hexInput
                            anchors.fill: parent; anchors.margins: 7; text: "#ffffff"; color: "#e8edf2"; selectByMouse: true; font.family: "monospace"
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/ }
                            function sync() { if (!activeFocus && stopList.currentItem) text = String(editPanel.second ? stopList.currentItem.secondColor : stopList.currentItem.stopColor) }
                            onEditingFinished: if (acceptableInput && stopList.currentIndex >= 0) paletteManager.editor.setStopColor(stopList.currentIndex, text, editPanel.second)
                            Connections { target: paletteManager.editor; function onDataChanged() { hexInput.sync() } }
                        }
                    }
                    Row { spacing: 8
                        Rectangle { width: 104; height: 28; radius: 4; color: paletteManager.activeIsFactoryPalette ? "#293541" : "#20262c"
                            Text { anchors.centerIn: parent; text: "Reset " + paletteManager.activeName; color: paletteManager.activeIsFactoryPalette ? "#dbe4ed" : "#65717d"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; enabled: paletteManager.activeIsFactoryPalette; onClicked: paletteManager.requestResetActive() } }
                        Rectangle { width: 104; height: 28; radius: 4; color: "#293541"
                            Text { anchors.centerIn: parent; text: "Reset all"; color: "#dbe4ed"; font.pixelSize: 10 }
                            MouseArea { anchors.fill: parent; onClicked: paletteManager.requestResetAll() } }
                    }
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; visible: paletteManager.confirmationRequired; z: 199 }
    Rectangle {
        visible: paletteManager.confirmationRequired; z: 200; anchors.centerIn: parent; width: 410; height: 155; radius: 8; color: "#202832"; border.color: "#526171"
        Column { anchors.fill: parent; anchors.margins: 18; spacing: 13
            Text { text: "Save changes to " + paletteManager.activeName + "?"; color: "#edf2f7"; font.pixelSize: 16; font.bold: true }
            Text { text: "Factory palettes are never overwritten. Save a separate .pal copy, discard the working changes, or keep editing."; width: parent.width; wrapMode: Text.WordWrap; color: "#aeb9c5"; font.pixelSize: 11 }
            Row { spacing: 10
                Rectangle { width: 104; height: 30; radius: 4; color: "#31557a"
                    Text { anchors.centerIn: parent; text: "Save a copy"; color: "white" }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(0) } }
                Rectangle { width: 90; height: 30; radius: 4; color: "#3b2c2c"
                    Text { anchors.centerIn: parent; text: "Discard"; color: "#f0d8d8" }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(1) } }
                Rectangle { width: 100; height: 30; radius: 4; color: "#303944"
                    Text { anchors.centerIn: parent; text: "Keep editing"; color: "#dce5ee" }
                    MouseArea { anchors.fill: parent; onClicked: paletteManager.resolveUnsavedChanges(2) } }
            }
        }
    }
    FileDialog { id: openDialog; title: "Open GRLevelX palette"; nameFilters: ["Palette files (*.pal)"]; onAccepted: paletteManager.openFile(selectedFile) }
    FileDialog { id: saveDialog; property bool pending: false; title: "Save palette as"; fileMode: FileDialog.SaveFile; nameFilters: ["Palette files (*.pal)"]; onAccepted: pending ? paletteManager.completePendingSave(selectedFile) : paletteManager.saveAs(selectedFile) }
}

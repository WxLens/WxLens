// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Popup {
    id: root
    anchors.centerIn: Overlay.overlay
    width: 560; height: 470
    modal: true; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: themeManager.surface; border.color: themeManager.border; radius: themeManager.cornerRadius }

    FileDialog {
        id: placefilePicker
        title: "Add placefile"
        nameFilters: ["Placefiles (*.txt *.placefile)", "All files (*)"]
        onAccepted: overlayManager.addPlacefile(selectedFile)
    }
    FileDialog {
        id: warningPicker
        title: "Import AWIPS warning file"
        nameFilters: ["Text products (*.txt)", "All files (*)"]
        onAccepted: overlayManager.importWarningFile(selectedFile)
    }

    Column {
        anchors.fill: parent; anchors.margins: 18; spacing: 12
        Row {
            width: parent.width
            Text { text: "Weather overlays"; color: themeManager.textPrimary; font.pixelSize: 19; font.bold: true }
            Item { width: parent.width - 190; height: 1 }
            Button { text: "Close"; onClicked: root.close() }
        }
        Row {
            spacing: 10
            CheckBox { text: "Warnings / watches"; checked: overlayManager.warningsVisible; onToggled: overlayManager.warningsVisible = checked }
            Button { text: overlayManager.refreshingWarnings ? "Refreshing…" : "Refresh live"; enabled: !overlayManager.refreshingWarnings; onClicked: overlayManager.refreshWarnings() }
            Button { text: "Import file"; onClicked: warningPicker.open() }
        }
        Rectangle { width: parent.width; height: 1; color: themeManager.border }
        Row {
            spacing: 10
            CheckBox { text: "Placefiles"; checked: overlayManager.placefilesVisible; onToggled: overlayManager.placefilesVisible = checked }
            Button { text: "Add local…"; onClicked: placefilePicker.open() }
        }
        Row {
            spacing: 8
            TextField { id: urlField; width: 390; placeholderText: "https://example.com/overlay.txt" }
            Button { text: "Add URL"; enabled: urlField.text.length > 0; onClicked: { overlayManager.addPlacefile(urlField.text); urlField.clear() } }
        }
        ListView {
            width: parent.width; height: 220; clip: true
            model: overlayManager.placefiles
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width; height: 50; color: index % 2 ? themeManager.elevatedSurface : "transparent"
                Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 160; elide: Text.ElideMiddle; text: modelData.title || modelData.source; color: modelData.error ? themeManager.danger : themeManager.textSecondary }
                Button { anchors.right: removeButton.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData.loading ? "…" : "Refresh"; enabled: !modelData.loading; onClicked: overlayManager.refreshPlacefile(index) }
                Button { id: removeButton; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Remove"; onClicked: overlayManager.removePlacefile(index) }
            }
        }
        Text { text: overlayManager.statusText; color: themeManager.textMuted; font.pixelSize: 11 }
    }
}

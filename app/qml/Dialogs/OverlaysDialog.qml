// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Popup {
    id: root
    anchors.centerIn: Overlay.overlay
    width: 560; height: 530
    modal: true; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: themeManager.surface; border.color: themeManager.border; radius: themeManager.cornerRadius }

    component OverlayButton: Button {
        id: button
        implicitWidth: Math.max(78, buttonText.implicitWidth + 24)
        implicitHeight: 34
        hoverEnabled: true

        contentItem: Text {
            id: buttonText
            text: button.text
            color: button.enabled ? themeManager.textPrimary : themeManager.textMuted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: themeManager.cornerRadius
            color: !button.enabled ? themeManager.surface
                                   : button.down ? themeManager.controlActive
                                                 : button.hovered ? themeManager.controlHover
                                                                  : themeManager.control
            border.width: button.visualFocus ? 2 : 1
            border.color: button.visualFocus ? themeManager.primary : themeManager.border
        }
    }

    component OverlayCheckBox: CheckBox {
        id: checkBox
        spacing: 8
        hoverEnabled: true

        indicator: Rectangle {
            implicitWidth: 20
            implicitHeight: 20
            x: checkBox.leftPadding
            y: (checkBox.height - height) / 2
            radius: 4
            color: !checkBox.enabled ? themeManager.surface
                                     : checkBox.down ? themeManager.controlActive
                                                     : checkBox.hovered ? themeManager.controlHover
                                                                        : themeManager.control
            border.width: checkBox.visualFocus ? 2 : 1
            border.color: checkBox.visualFocus ? themeManager.primary : themeManager.border

            Text {
                anchors.centerIn: parent
                text: "✓"
                visible: checkBox.checked
                color: checkBox.enabled ? themeManager.textPrimary : themeManager.textMuted
                font.pixelSize: 14
                font.bold: true
            }
        }

        contentItem: Text {
            leftPadding: checkBox.indicator.width + checkBox.spacing
            text: checkBox.text
            color: checkBox.enabled ? themeManager.textPrimary : themeManager.textMuted
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
    }

    component OverlayTextField: TextField {
        id: textField
        implicitHeight: 36
        leftPadding: 10
        rightPadding: 10
        color: textField.enabled ? themeManager.textPrimary : themeManager.textMuted
        placeholderTextColor: themeManager.textSecondary
        selectionColor: themeManager.primary
        selectedTextColor: themeManager.textPrimary

        background: Rectangle {
            radius: themeManager.cornerRadius
            color: textField.enabled ? themeManager.control : themeManager.surface
            border.width: textField.activeFocus ? 2 : 1
            border.color: textField.activeFocus ? themeManager.primary : themeManager.border
        }
    }

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
            OverlayButton { text: "Close"; onClicked: root.close() }
        }
        Row {
            spacing: 10
            OverlayCheckBox { text: "Warnings / watches"; checked: overlayManager.warningsVisible; onToggled: overlayManager.warningsVisible = checked }
            OverlayButton { text: overlayManager.refreshingWarnings ? "Refreshing…" : "Refresh live"; enabled: !overlayManager.refreshingWarnings; onClicked: overlayManager.refreshWarnings() }
            OverlayButton { text: "Import file"; onClicked: warningPicker.open() }
        }
        Rectangle { width: parent.width; height: 1; color: themeManager.border }
        Row {
            spacing: 10
            OverlayCheckBox { text: "Placefiles"; checked: overlayManager.placefilesVisible; onToggled: overlayManager.placefilesVisible = checked }
            OverlayButton { text: "Add local…"; onClicked: placefilePicker.open() }
        }
        Text { text: "Add a placefile from a web address"; color: themeManager.textSecondary; font.pixelSize: 12 }
        Row {
            spacing: 8
            OverlayTextField { id: urlField; width: 390; placeholderText: "https://example.com/overlay.txt"; Accessible.name: "Placefile web address" }
            OverlayButton { text: "Add from web"; enabled: urlField.text.trim().length > 0; onClicked: { overlayManager.addPlacefile(urlField.text.trim()); urlField.clear() } }
        }
        ListView {
            width: parent.width; height: 220; clip: true
            model: overlayManager.placefiles
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width; height: 50; color: index % 2 ? themeManager.elevatedSurface : "transparent"
                Text { anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 190; elide: Text.ElideMiddle; text: modelData.title || modelData.source; color: modelData.error ? themeManager.danger : themeManager.textSecondary }
                OverlayButton { anchors.right: removeButton.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData.loading ? "Refreshing…" : "Refresh"; enabled: !modelData.loading; onClicked: overlayManager.refreshPlacefile(index) }
                OverlayButton { id: removeButton; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Remove"; onClicked: overlayManager.removePlacefile(index) }
            }
        }
        Text { width: parent.width; text: overlayManager.statusText; color: themeManager.textSecondary; font.pixelSize: 12; wrapMode: Text.Wrap }
    }
}

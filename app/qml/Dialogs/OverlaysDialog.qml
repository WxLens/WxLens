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
            // textMuted, not border: an unchecked box is identified by its outline alone, so that
            // outline needs WCAG 1.4.11's 3:1 against the surface (border is ~1.4:1 in both
            // bundled themes; textMuted clears 4:1 in both - see the contrast audit recorded in
            // docs/phase1-ux-feedback-2026-08-31.md).
            border.width: checkBox.visualFocus ? 2 : 1
            border.color: checkBox.visualFocus ? themeManager.primary : themeManager.textMuted

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
            // Same 3:1 boundary requirement as the checkbox above: an empty field is only its outline.
            border.width: textField.activeFocus ? 2 : 1
            border.color: textField.activeFocus ? themeManager.primary : themeManager.textMuted
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
        Row {
            spacing: 10
            OverlayCheckBox { text: "Radar sites"; checked: appSettings.radarSitesVisible; onToggled: appSettings.radarSitesVisible = checked }
            OverlayCheckBox { text: "Include TDWR"; enabled: appSettings.radarSitesVisible; checked: appSettings.tdwrSitesVisible; onToggled: appSettings.tdwrSitesVisible = checked }
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
                Accessible.role: Accessible.ListItem
                Accessible.name: (modelData.title || modelData.source) + (modelData.error ? ", failed: " + modelData.error : "")
                Column {
                    anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 190; spacing: 2
                    Text { width: parent.width; elide: Text.ElideMiddle; text: modelData.title || modelData.source; color: themeManager.textSecondary; font.pixelSize: 12 }
                    // A failed placefile keeps its name readable and says what went wrong on its own
                    // line. The ⚠ carries the state without relying on colour (WCAG 1.4.1), and
                    // `warning` is the one status token that reaches 4.5:1 on both row backgrounds
                    // in both bundled themes - `danger` does not in Operational Dark.
                    Text { visible: modelData.error !== ""; width: parent.width; elide: Text.ElideRight; text: "⚠ " + modelData.error; color: themeManager.warning; font.pixelSize: 11 }
                }
                OverlayButton { anchors.right: removeButton.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData.loading ? "Refreshing…" : "Refresh"; enabled: !modelData.loading; onClicked: overlayManager.refreshPlacefile(index) }
                OverlayButton { id: removeButton; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Remove"; onClicked: overlayManager.removePlacefile(index) }
            }
        }
        Text { width: parent.width; text: overlayManager.statusText; color: themeManager.textSecondary; font.pixelSize: 12; wrapMode: Text.Wrap }
    }
}

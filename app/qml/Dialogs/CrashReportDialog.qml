// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import WxLens.App

Rectangle {
    id: root
    anchors.fill: parent
    visible: crashReports.visible
    z: 150
    color: "#99000000"
    focus: visible
    onVisibleChanged: { if (visible) forceActiveFocus() }

    Keys.onEscapePressed: (event) => {
        crashReports.dismiss()
        event.accepted = true
    }
    MouseArea {
        anchors.fill: parent
        preventStealing: true
        onWheel: (wheel) => wheel.accepted = true
    }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(760, root.width - 40)
        height: Math.min(650, root.height - 40)
        radius: themeManager.cornerRadius
        color: themeManager.surface
        border.color: themeManager.border

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Text {
                text: crashReports.hasReport ? "WxLens found a crash report" : "Crash reports"
                color: themeManager.textPrimary
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                id: explanation
                width: parent.width
                wrapMode: Text.WordWrap
                color: themeManager.textSecondary
                font.pixelSize: 12
                text: crashReports.hasReport ?
                    "Review the report below before sharing it. It contains crash details and software versions. Raw logs, memory dumps, personal file paths and settings are not attached." :
                    "No supported crash report was found. On macOS, reports can take a moment to appear; close this panel and open it again to check."
            }
            Text {
                id: destinationLabel
                width: parent.width
                wrapMode: Text.WordWrap
                color: themeManager.textSecondary
                font.pixelSize: 12
                text: crashReports.destination.length > 0 ?
                    "Send shares this report privately with the WxLens developer through Sentry (" + crashReports.destination + "). Like any online service, it receives your connection’s IP address. Nothing is sent automatically." :
                    "Online reporting is not configured in this build. You can save the report and share it with the developer."
            }
            ScrollView {
                id: preview
                width: parent.width
                height: Math.max(80, parent.height - explanation.height - destinationLabel.height - statusText.height - 128)
                clip: true
                contentWidth: availableWidth
                TextArea {
                    width: preview.availableWidth
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    textFormat: TextEdit.PlainText
                    text: crashReports.reportText
                    color: themeManager.textPrimary
                    font.family: "monospace"
                    font.pixelSize: 11
                    background: Rectangle { color: themeManager.background }
                    Accessible.name: "Crash report preview"
                }
            }
            Text {
                id: statusText
                width: parent.width
                text: crashReports.status
                wrapMode: Text.WordWrap
                color: themeManager.textSecondary
                font.pixelSize: 12
            }
            Row {
                spacing: 10
                WxButton {
                    text: "Close"
                    enabled: !crashReports.sending
                    onClicked: crashReports.dismiss()
                }
                WxButton {
                    text: "Save report…"
                    enabled: crashReports.hasReport && !crashReports.sending
                    onClicked: saveDialog.open()
                }
                WxButton {
                    text: crashReports.sending ? "Sending…" : "Send crash report"
                    enabled: crashReports.canSend
                    highlighted: true
                    onClicked: crashReports.send()
                }
            }
        }
    }
    FileDialog {
        id: saveDialog
        title: "Save crash report"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Crash report (*.json)"]
        defaultSuffix: "json"
        onAccepted: crashReports.save(selectedFile)
    }
}

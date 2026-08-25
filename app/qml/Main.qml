// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Window

import Nimbus.App

Window {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: "Nimbus"
    color: themeManager.background

    Column {
        anchors.fill: parent
        spacing: 0

        TopBar {
            width: parent.width
            onSettingsRequested: settingsDialog.open()
            onPaletteRequested: paletteDialog.open()
        }

        Row {
            width: parent.width
            height: parent.height - 48
            spacing: 0

            SideRail {
                height: parent.height
                onConfigureRequested: (sectionId) => settingsDialog.openAt(sectionId)
            }

            PaneGrid {
                width: parent.width - 56
                height: parent.height
                model: paneGridModel
                onConfigureRequested: (sectionId) => settingsDialog.openAt(sectionId)
            }
        }
    }

    // Above everything, and last so it stacks over the chrome as well as the panes. Deep-links
    // from quick controls arrive as openAt(sectionId) - see §4.5 and SettingsDialog's comment for
    // why the id, not an index, is the addressing scheme.
    SettingsDialog {
        id: settingsDialog
    }

    PaletteDialog { id: paletteDialog }
}

// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Window

import WxLens.App

Window {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: "WxLens"
    color: themeManager.background

    Column {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            width: parent.width
            onSettingsRequested: settingsDialog.open()
            onPaletteRequested: paletteDialog.open()
            onMapDetailsRequested: settingsDialog.openAt("map-details")
            onSavedPlacesRequested: savedPlacesDialog.open()
            onOverlaysRequested: overlaysDialog.open()
            onHelpRequested: helpDialog.open()
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height

            PaneGrid {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: appSettings.controlBarDocked ? bottomBar.top : parent.bottom
                model: paneGridModel
                onConfigureRequested: (sectionId) => settingsDialog.openAt(sectionId)
            }

            BottomControlBar {
                id: bottomBar
                gridModel: paneGridModel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: appSettings.controlBarDocked ? 0 : 14
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
    SavedPlacesDialog { id: savedPlacesDialog }
    OverlaysDialog { id: overlaysDialog }
    HelpDialog { id: helpDialog }

    Shortcut { sequence: "F1"; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+/"; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+,"; onActivated: settingsDialog.open() }
    Shortcut { sequence: "Ctrl+F"; onActivated: savedPlacesDialog.openAndFocusSearch() }
}

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
    color: "#101418"

    Column {
        anchors.fill: parent
        spacing: 0

        TopBar {
            width: parent.width
        }

        Row {
            width: parent.width
            height: parent.height - 48
            spacing: 0

            SideRail {
                height: parent.height
            }

            PaneHost {
                width: parent.width - 56
                height: parent.height
            }
        }
    }
}

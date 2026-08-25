// SPDX-License-Identifier: MIT
import QtQuick

// A labelled one-of-N choice for the settings surface.
//
// Segmented buttons rather than a dropdown: every one of these has three or fewer options, and a
// dropdown hides the alternatives behind a click, which is the opposite of what a settings page is
// for - you are there precisely to see what the choices are. Each carries an explanation line,
// because a preference nobody understands is one nobody changes (§5.3: approachable by not
// surfacing complexity until asked, not by hiding what things mean).
Column {
    id: root

    property string label: ""
    property string explanation: ""
    property var options: []
    property int currentIndex: 0

    signal selected(int index)

    spacing: 4

    Text {
        text: root.label
        color: themeManager.textPrimary
        font.pixelSize: 12
    }

    Row {
        spacing: 6

        Repeater {
            model: root.options

            delegate: Rectangle {
                required property var modelData
                required property int index

                readonly property bool active: root.currentIndex === index

                width: optionText.implicitWidth + 20
                height: 26
                radius: themeManager.cornerRadius
                color: active ? themeManager.controlActive : (optionArea.containsMouse ? themeManager.controlHover : themeManager.control)
                border.color: active ? themeManager.primary : themeManager.border
                border.width: 1

                Text {
                    id: optionText
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: parent.active ? themeManager.textPrimary : themeManager.textMuted
                    font.pixelSize: 11
                }

                MouseArea {
                    id: optionArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selected(parent.index)
                }
            }
        }
    }

    Text {
        visible: root.explanation !== ""
        width: root.width
        text: root.explanation
        color: themeManager.textMuted
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }
}

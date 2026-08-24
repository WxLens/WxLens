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
        color: "#dce6f2"
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
                radius: 4
                color: active ? "#2b3a4d" : (optionArea.containsMouse ? "#20262e" : "#1a1f26")
                border.color: active ? "#4a7ab0" : "#2f3742"
                border.width: 1

                Text {
                    id: optionText
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: parent.active ? "#dce6f2" : "#8d99a8"
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
        color: "#5f6b7a"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }
}

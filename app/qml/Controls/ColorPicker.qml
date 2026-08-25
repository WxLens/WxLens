// SPDX-License-Identifier: MIT
import QtQuick

// Shared by palette stops now and saved-place/group colors in slice 15. The picker is reusable;
// palette, theme and object-color storage remain deliberately separate systems.
Rectangle {
    id: root
    property color colorValue: "#ffffff"
    signal colorEdited(color value)
    width: 220
    height: 150
    radius: 6
    color: themeManager.elevatedSurface
    border.color: themeManager.border

    function channelColor(channel, value) {
        var r = Math.round(root.colorValue.r * 255)
        var g = Math.round(root.colorValue.g * 255)
        var b = Math.round(root.colorValue.b * 255)
        if (channel === 0) r = value
        if (channel === 1) g = value
        if (channel === 2) b = value
        return Qt.rgba(r / 255, g / 255, b / 255, root.colorValue.a)
    }

    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        Rectangle { width: parent.width; height: 32; radius: 4; color: root.colorValue }
        Repeater {
            model: ["Red", "Green", "Blue"]
            delegate: Row {
                required property string modelData
                required property int index
                spacing: 8
                Text { width: 38; text: parent.modelData; color: themeManager.textSecondary; font.pixelSize: 10 }
                Rectangle {
                    width: 140; height: 14; radius: 7; color: themeManager.control
                    Rectangle {
                        width: 12; height: 18; radius: 6; y: -2
                        x: (index === 0 ? root.colorValue.r : index === 1 ? root.colorValue.g : root.colorValue.b) * (parent.width - width)
                        color: themeManager.textPrimary
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: (mouse) => update(mouse.x)
                        onPositionChanged: (mouse) => { if (pressed) update(mouse.x) }
                        function update(x) {
                            var value = Math.round(Math.max(0, Math.min(1, x / width)) * 255)
                            root.colorValue = root.channelColor(index, value)
                            root.colorEdited(root.colorValue)
                        }
                    }
                }
            }
        }
    }
}

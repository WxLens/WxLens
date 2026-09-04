// SPDX-License-Identifier: MIT
import QtQuick

/**
 * One row of a themed dropdown or overflow menu, announced as a menu item and operable from the
 * keyboard. See WxButton for why these shared controls exist rather than per-site MouseAreas.
 *
 * `detail` is the smaller second line some menus carry (units, the current default, a product
 * code). It is appended to what a screen reader announces, because it usually carries the fact
 * that distinguishes one row from another.
 */
Rectangle {
    id: root

    property string text: ""
    property string detail: ""
    property bool selected: false
    property bool enabled: true
    property string name: root.detail === "" ? root.text : root.text + ", " + root.detail

    signal triggered()

    implicitHeight: root.detail === "" ? 30 : 36
    height: implicitHeight
    radius: themeManager.cornerRadius
    color: root.selected
               ? themeManager.controlActive
               : mouseArea.containsMouse ? themeManager.controlHover : "transparent"
    border.width: root.activeFocus ? 2 : 0
    border.color: themeManager.primary

    Accessible.role: Accessible.MenuItem
    Accessible.name: root.name
    Accessible.onPressAction: root.trigger()
    activeFocusOnTab: root.enabled

    function trigger() {
        if (root.enabled) root.triggered()
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            width: parent.width
            text: root.text
            color: root.enabled ? themeManager.textPrimary : themeManager.textMuted
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: root.detail !== ""
            text: root.detail
            color: themeManager.textMuted
            font.pixelSize: 9
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.trigger()
    }

    Keys.onSpacePressed: root.trigger()
    Keys.onReturnPressed: root.trigger()
    Keys.onEnterPressed: root.trigger()
}

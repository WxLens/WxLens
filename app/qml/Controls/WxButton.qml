// SPDX-License-Identifier: MIT
import QtQuick
// ToolTip is an attached property from Controls; without this import it is silently unknown and
// fails at *runtime*, not at build time.
import QtQuick.Controls

/**
 * A themed, accessible push button.
 *
 * WxLens draws its own controls (Rectangle + Text + MouseArea) rather than using Qt Quick
 * Controls, which is why this component exists: a bare MouseArea is invisible to a screen reader
 * and unreachable from the keyboard, so every hand-rolled control had to remember to add a role,
 * a name, tab focus and Space/Return handling - and none of them did (89 such controls as of
 * 2026-09-03). Putting it here means a control is accessible because of what it *is*, not because
 * whoever added it remembered.
 *
 * Set `text` for a labelled button. For an icon or glyph, set `text` to the glyph and `name` to
 * what it does ("Settings", not "gear") - the glyph is meaningless read aloud.
 */
Rectangle {
    id: root

    property string text: ""
    /// What a screen reader announces. Defaults to the visible label, which is right for a
    /// labelled button and wrong for a glyph - hence the override.
    property string name: root.text
    /// Longer spoken description, and the tooltip text when `tooltip` is left empty.
    property string description: ""
    property string tooltip: root.description
    property bool flat: false
    property bool highlighted: false
    /**
     * Set when `highlighted` means "this is the chosen one of a set" - the selected radar product,
     * the active pane, the current help page - rather than mere visual emphasis on a primary
     * action. The highlight colour alone cannot convey that to a screen reader, so this exposes it
     * as the checked state of `Accessible.role: Button` (the convention for a button that acts as
     * one option of several, rather than the ListItem `Accessible.selected` a real list would use).
     * Defaults off because most buttons are plain actions, and announcing every one of those as
     * "not checked" is noise rather than information.
     */
    property bool selectable: false
    // No `enabled` property here: Item already has one, and redeclaring it shadowed the base
    // member (qt.qml.propertyCache warned about exactly that) while also breaking the automatic
    // propagation that disables child items.
    property alias hovered: mouseArea.containsMouse
    property alias font: label.font
    property int horizontalPadding: 12

    signal clicked()

    implicitWidth: Math.max(28, label.implicitWidth + horizontalPadding * 2)
    implicitHeight: 28
    width: implicitWidth
    height: implicitHeight
    radius: themeManager.cornerRadius

    color: !root.enabled
               ? "transparent"
               : root.highlighted
                   ? themeManager.controlActive
                   : mouseArea.containsPress
                       ? themeManager.controlActive
                       : mouseArea.containsMouse
                           ? themeManager.controlHover
                           : root.flat ? "transparent" : themeManager.control

    // The focus ring has to be visible on a flat button too, or keyboard users lose their place
    // crossing the toolbar.
    border.width: root.activeFocus ? 2 : (root.flat ? 0 : 1)
    border.color: root.activeFocus ? themeManager.primary : themeManager.border

    Accessible.role: Accessible.Button
    Accessible.name: root.name
    Accessible.description: root.description
    Accessible.checkable: root.selectable
    Accessible.checked: root.selectable && root.highlighted
    Accessible.onPressAction: root.trigger()
    activeFocusOnTab: root.enabled

    function trigger() {
        if (root.enabled) root.clicked()
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.enabled ? themeManager.textPrimary : themeManager.textMuted
        font.pixelSize: 11
    }

    MouseArea {
        id: mouseArea
        property bool containsPress: pressed && containsMouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.trigger()
    }

    Keys.onSpacePressed: root.trigger()
    Keys.onReturnPressed: root.trigger()
    Keys.onEnterPressed: root.trigger()

    ToolTip.visible: mouseArea.containsMouse && root.tooltip !== ""
    ToolTip.delay: 450
    ToolTip.text: root.tooltip
}

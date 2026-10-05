import QtQuick

Item {
    id: root
    property int frameCount: 0
    property int selectedIndex: -1
    property color trackColor: "#657080"
    property color handleColor: "#64b5f6"
    signal seekRequested(int index)
    implicitWidth: 180
    implicitHeight: 32
    activeFocusOnTab: true
    enabled: frameCount > 0
    Accessible.role: Accessible.Slider
    Accessible.name: "Radar playback frame"
    Accessible.description: "Use left and right arrows to step through available scans"

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: 7; width: parent.width - 14; height: 3
        radius: 1.5; color: root.trackColor
    }
    Rectangle {
        width: 12; height: 12; radius: 6
        anchors.verticalCenter: parent.verticalCenter
        x: root.frameCount > 1 ? 1 + (parent.width - 14) * Math.max(0, root.selectedIndex) / (root.frameCount - 1) : 1
        color: root.handleColor
        border.width: root.activeFocus ? 2 : 0
        border.color: "white"
    }
    MouseArea {
        anchors.fill: parent
        preventStealing: true
        property bool dragging: false
        function seekAt(x) {
            root.seekRequested(Math.round(Math.max(0, Math.min(1, (x - 7) / Math.max(1, width - 14))) * (root.frameCount - 1)))
        }
        onPressed: mouse => { root.forceActiveFocus(); dragging = true; seekAt(mouse.x) }
        onPositionChanged: mouse => { if (dragging) seekAt(mouse.x) }
        onReleased: mouse => { seekAt(mouse.x); dragging = false }
        onCanceled: dragging = false
    }
    // Keys has named handlers for Left/Right but none for Home/End, so one handler covers all
    // four rather than mixing two styles for the same gesture.
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left) seekRequested(Math.max(0, selectedIndex - 1))
        else if (event.key === Qt.Key_Right) seekRequested(Math.min(frameCount - 1, selectedIndex + 1))
        else if (event.key === Qt.Key_Home) seekRequested(0)
        else if (event.key === Qt.Key_End) seekRequested(frameCount - 1)
        else return
        event.accepted = true
    }
}

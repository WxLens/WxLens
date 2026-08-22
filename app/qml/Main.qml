import QtQuick
import QtQuick.Window

Window {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: "Nimbus"
    color: "#101418"

    Text {
        anchors.centerIn: parent
        text: "Nimbus — Phase 0 shell\n(no radar/map layer yet)"
        color: "#c8d0d8"
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 20
    }
}

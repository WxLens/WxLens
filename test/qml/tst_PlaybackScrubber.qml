import QtQuick
import QtTest
import WxLens.App

TestCase {
    name: "PlaybackScrubber"
    when: windowShown
    // A TestCase is invisible by default, and an invisible item is not hit-tested:
    // without this every synthesized mouse event lands nowhere and the test still passes.
    visible: true
    width: 400; height: 100

    PlaybackScrubber {
        id: scrubber
        width: 300; height: 40
        frameCount: 7
        onSeekRequested: index => selectedIndex = index
    }
    function init() { scrubber.selectedIndex = 0 }
    // mouseMove defaults to Qt.NoButton, which is a hover the MouseArea never sees;
    // a real drag has to carry the held button through every move.
    function test_pressDragRelease() {
        mousePress(scrubber, 7, 20)
        mouseMove(scrubber, 150, 20, 30, Qt.LeftButton)
        compare(scrubber.selectedIndex, 3)
        mouseMove(scrubber, 293, 20, 30, Qt.LeftButton)
        mouseRelease(scrubber, 293, 20)
        compare(scrubber.selectedIndex, 6)
    }
    function test_keyboard() {
        scrubber.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(scrubber.selectedIndex, 1)
        keyClick(Qt.Key_End)
        compare(scrubber.selectedIndex, 6)
        keyClick(Qt.Key_Left)
        compare(scrubber.selectedIndex, 5)
    }
}

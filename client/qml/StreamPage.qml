import QtQuick
import QtQuick.Controls
import VideoStream

EasyPage {
    id: root

    signal back
    overlayHeaderBar: true

    property bool headerPointerInArea: false

    function showHeaderBar() {
        headerBarVisible = true
        hideHeaderBarTimer.restart()
    }

    Timer {
        id: hideHeaderBarTimer
        interval: 3000
        repeat: false

        onTriggered: {
            if (!root.headerPointerInArea) {
                root.headerBarVisible = false
            }
        }
    }

    headerBar.leftContent: EasyButton {
        display: AbstractButton.IconOnly
        icon.source: "qrc:/assets/go-previous.svg"

        height: headerBar.centerHeight
        width: headerBar.centerHeight

        onClicked: root.back()
    }

    overlay: Item {
        anchors.fill: parent

        MouseArea {
            id: headerHotZone

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: root.headerBar.implicitHeight

            hoverEnabled: true
            acceptedButtons: root.headerBarVisible ? Qt.NoButton : Qt.AllButtons

            onClicked: root.showHeaderBar()
            onEntered: {
                root.headerPointerInArea = true
                root.showHeaderBar()
            }
            onExited: {
                root.headerPointerInArea = false
                hideHeaderBarTimer.restart()
            }
            onPressed: root.showHeaderBar()
        }
    }

    content: VideoFrame {
        id: stream

        Component.onCompleted: {
            networkLink().setItem(stream)
        }
    }

    onVisibleChanged: {
        if (root.visible) {
            root.headerBarVisible = true
            hideHeaderBarTimer.restart()
        } else {
            hideHeaderBarTimer.stop()
            root.headerPointerInArea = false
            root.headerBarVisible = true
        }
    }
}

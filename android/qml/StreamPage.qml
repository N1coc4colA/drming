import QtQml
import QtQuick.Controls
import VideoStream

EasyPage {
    id: root

    signal back

    headerBar.leftContent: EasyButton {
        display: AbstractButton.IconOnly
        icon.source: "qrc:/assets/go-previous.svg"

        height: headerBar.centerHeight
        width: headerBar.centerHeight

        onClicked: root.back()
    }

    content: VideoFrame {
        id: stream

        Connections {
            target: networkLink()

            function onImageReady(image) {
                stream.setImage(image);
            }
        }
    }
}

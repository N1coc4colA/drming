import QtQml
import QtQuick.Controls
import VideoStream

EasyPage {
    id: root

    signal back

    headerBar.leftContent: EasyButton {
        icon.source: "qrc:/assets/go-previous.svg"
        display: AbstractButton.IconOnly

        onClicked: root.back()

        height: headerBar.centerHeight
        width: headerBar.centerHeight
    }

    content: VideoFrame {
        id: stream

        Connections {
            target: networkLink

            function onImageReady(image) {
                stream.setImage(image);
            }
        }
    }
}

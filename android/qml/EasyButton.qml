import QtQuick.Controls
import QtQuick

Button {
    id: root

    property real bounding: 20
    property int animationsDuration: 100

    icon.height: bounding
    icon.width: bounding
    icon.color: "transparent"

    background: Rectangle {
        id: backgroundRectangle
        color: palette.mid
        radius: 8
        border.color: palette.dark
        border.width: 1
    }

    onPressed: anim.start()

    TapAnimation {
        id: anim
        duration: animationsDuration
        container: root
        background: backgroundRectangle
    }
}

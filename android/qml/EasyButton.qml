import QtQuick.Controls
import QtQuick

Button {
    id: root

    property real bounding: 20
    property int animationsDuration: 100
    property color backgroundColor: palette.mid
    property color borderColor: palette.dark
    property color highlightColor: palette.highlight

    icon.height: bounding
    icon.width: bounding
    icon.color: "transparent"

    background: Rectangle {
        id: backgroundRectangle
        color: backgroundColor
        radius: 8
        border.color: borderColor
        border.width: 1
    }

    onPressed: anim.start()

    TapAnimation {
        id: anim
        duration: animationsDuration
        container: root
        background: backgroundRectangle
        normalBackground: backgroundColor
        highlightBackground: highlightColor
    }
}

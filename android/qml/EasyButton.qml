import QtQuick.Controls
import QtQuick

Button {
    id: root

    property real bounding: 20 * GlobalVars.scaling
    property int animationsDuration: 100
    property color backgroundColor: palette.mid
    property color borderColor: palette.dark
    property color highlightColor: palette.highlight
    property color color: palette.buttonText

    icon.height: bounding
    icon.width: bounding
    icon.color: "transparent"

    TapAnimation {
        id: anim
        duration: animationsDuration
        container: root

        background: backgroundRectangle
        highlightBackground: highlightColor
        normalBackground: backgroundColor
    }

    background: Rectangle {
        id: backgroundRectangle
        color: backgroundColor

        border.color: borderColor
        border.width: 1
        radius: GlobalVars.standardRounding
        height: root.height
        width: root.width
    }

    onPressed: anim.start()
    onColorChanged: {
        root.contentItem.color = root.color
    }
    Component.onCompleted: {
         root.contentItem.color = root.color
    }
}

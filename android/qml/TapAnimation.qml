import QtQuick

ParallelAnimation {
    id: root

    property Item background: null
    property Item container: null
    property int duration: 200

    property color normalBackground: palette.mid
    property color highlightBackground: palette.highlight

    SequentialAnimation {
        PropertyAnimation {
            target: root.container
            property: "scale"
            to: 1.1
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }

        PropertyAnimation {
            target: root.container
            property: "scale"
            to: 1.0
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }
    }
    SequentialAnimation {
        PropertyAnimation {
            target: root.background
            property: "color"
            to: highlightBackground
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }

        PropertyAnimation {
            target: root.background
            property: "color"
            to: backgroundColor
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }
    }
}

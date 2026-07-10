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
            duration: root.duration/2
            easing.type: Easing.InOutQuad
            to: 1.1
        }

        PropertyAnimation {
            target: root.container
            property: "scale"
            duration: root.duration/2
            easing.type: Easing.InOutQuad
            to: 1.0
        }
    }
    SequentialAnimation {
        PropertyAnimation {
            target: root.background
            property: "color"
            duration: root.duration/2
            easing.type: Easing.InOutQuad
            to: highlightBackground
        }

        PropertyAnimation {
            target: root.background
            property: "color"
            duration: root.duration/2
            easing.type: Easing.InOutQuad
            to: backgroundColor
        }
    }
}

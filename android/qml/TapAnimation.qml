import QtQuick

ParallelAnimation {
    id: root

    property Item background: null
    property Item container: null
    property int duration: 200

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
            to: palette.highlight
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }

        PropertyAnimation {
            target: root.background
            property: "color"
            to: palette.mid
            duration: root.duration/2
            easing.type: Easing.InOutQuad
        }
    }
}

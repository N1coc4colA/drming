import QtQuick.Controls.Basic as Base
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

Popup {
    id: root
    dim: true
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    implicitWidth: contentLayout.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentLayout.implicitHeight + topPadding + bottomPadding
    padding: 16

    x: (window.width - width)/2
    y: (window.height - height)/2

    property var blurSource: null
    property string title: ""
    property int animationsDuration: 200

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: animationsDuration
            }
            NumberAnimation {
                property: "scale"
                from: 0.4
                to: 1.0
                easing.type: Easing.OutBack
                duration: animationsDuration
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: animationsDuration
            }
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.8
                duration: animationsDuration
            }
        }
    }

    Overlay.modal: Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: "black"
        }
        MultiEffect {
            anchors.fill: parent
            source: root.blurSource
            blurEnabled: root.blurSource !== null
            blur: 1.0
            blurMax: 64
            colorizationColor: palette.text
            colorization: 0.2
            autoPaddingEnabled: false
        }
    }

    background: Rectangle {
        radius: 8
        color: palette.window
        border.width: 1
        border.color: palette.mid
        clip: true
        width: root.width
        height: root.height
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: 8

        Label {
            id: titleLabel
            text: root.title
            visible: root.title !== ""
            Layout.fillWidth: true
            horizontalAlignment: Qt.AlignHCenter
            font.bold: true
        }

        Loader {
            id: contentLoader
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            Layout.preferredWidth:  item ? item.implicitWidth  : 0
        }

        Loader {
            id: footerLoader
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            Layout.preferredWidth:  item ? item.implicitWidth  : 0
        }
    }

    property alias content: contentLoader.sourceComponent
    property alias footer:  footerLoader.sourceComponent
}

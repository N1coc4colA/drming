import QtQuick.Controls.Basic as Base
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

Popup {
    id: root
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    dim: true
    modal: true
    focus: true
    padding: 16

    x: (window.width - width)/2
    y: (window.height - height)/2
    implicitWidth: contentLayout.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentLayout.implicitHeight + topPadding + bottomPadding

    property string title: ""
    property int animationsDuration: 200

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                duration: animationsDuration
                property: "opacity"
                from: 0.0
                to: 1.0
            }
            NumberAnimation {
                duration: animationsDuration
                property: "scale"
                from: 0.4
                to: 1.0

                easing.type: Easing.OutBack
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                duration: animationsDuration
                property: "opacity"
                from: 1.0
                to: 0.0
            }
            NumberAnimation {
                duration: animationsDuration
                property: "scale"
                from: 1.0
                to: 0.8
            }
        }
    }

    Overlay.modal: Item {
        anchors.fill: parent

        Rectangle {
            color: "black"

            anchors.fill: parent
        }
        MultiEffect {
            autoPaddingEnabled: false
            blur: 1.0
            blurEnabled: GlobalVars.shaderBlurSource !== null
            blurMax: 64
            colorization: 0.2
            colorizationColor: palette.text
            source: GlobalVars.shaderBlurSource

            anchors.fill: parent
        }
    }

    background: Rectangle {
        clip: true
        color: palette.window
        width: root.width
        height: root.height
        radius: 8

        border.width: 1
        border.color: palette.mid
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: 8

        Label {
            id: titleLabel
            horizontalAlignment: Qt.AlignHCenter
            text: root.title
            visible: root.title !== ""

            font.bold: true
            Layout.fillWidth: true
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

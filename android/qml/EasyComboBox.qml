pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control
    spacing: 5

    delegate: ItemDelegate {
        id: delegate
        height: 28
        width: control.popup.width - control.popup.padding*2
        highlighted: control.highlightedIndex === index

        required property var model
        required property int index

        contentItem: Text {
            color: palette.text
            elide: Text.ElideRight
            font: control.font
            text: delegate.model[control.textRole]
            verticalAlignment: Text.AlignVCenter

            anchors.fill: delegate.background
            anchors.margins: 6
        }

        background: Rectangle {
            color: control.visualFocus
                ? (control.pressed ? palette.highlight : palette.dark)
                : (control.down ? palette.highlight : "transparent")
            radius: 5
            visible: control.down || control.highlighted || control.visualFocus

            anchors.fill: delegate
        }
    }

    indicator: Image {
        id: canvas
        x: control.width - width - control.rightPadding/2
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 16
        height: 16
        source: "qrc:/assets/go-down.svg"

        sourceSize.width: 18
        sourceSize.height: 16
    }

    contentItem: Text {
        rightPadding: control.indicator.width + control.spacing*2
        leftPadding: control.spacing
        color: control.pressed ? palette.highlightedText : palette.text
        elide: Text.ElideRight
        text: control.displayText
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: control.pressed ? palette.highlight : palette.mid
        implicitWidth: 100
        implicitHeight: 32
        radius: 8

        border.color: control.pressed ? palette.highlight : palette.dark
        border.width: control.visualFocus ? 2 : 1
    }

    popup: Popup {
        y: control.height + 2
        width: control.width + padding*2
        height: Math.min(contentItem.implicitHeight, control.Window.height - topMargin - bottomMargin) + padding*2
        padding: 5

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            currentIndex: control.highlightedIndex
            model: control.popup.visible ? control.delegateModel : null

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: palette.mid
            radius: 8

            border.color: palette.dark
            border.width: 1
        }
    }
}

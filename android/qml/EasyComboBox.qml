pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control
    spacing: GlobalVars.innerSpacing

    delegate: ItemDelegate {
        id: delegate
        highlighted: control.highlightedIndex === index

        height: 28
        width: control.popup.width - control.popup.padding*2

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
            radius: GlobalVars.innerRounding
            visible: control.down || control.highlighted || control.visualFocus

            anchors.fill: delegate
        }
    }

    indicator: Image {
        id: canvas
        source: "qrc:/assets/go-down.svg"

        x: control.width - width - control.rightPadding/2
        y: control.topPadding + (control.availableHeight - height) / 2
        height: 16
        width: 16

        sourceSize.height: 16
        sourceSize.width: 18
    }

    contentItem: Text {
        color: control.pressed ? palette.highlightedText : palette.text
        elide: Text.ElideRight
        text: control.displayText
        verticalAlignment: Text.AlignVCenter

        leftPadding: control.spacing
        rightPadding: control.indicator.width + control.spacing*2
    }

    background: Rectangle {
        color: control.pressed ? palette.highlight : palette.mid

        border.color: control.pressed ? palette.highlight : palette.dark
        border.width: control.visualFocus ? 2 : 1
        implicitHeight: 32
        implicitWidth: 100
        radius: GlobalVars.standardRounding
    }

    popup: Popup {
        y: control.height + 2
        height: Math.min(contentItem.implicitHeight, control.Window.height - topMargin - bottomMargin) + GlobalVars.innerSpacing*2
        width: control.width + padding*2
        padding: GlobalVars.innerSpacing

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight

            currentIndex: control.highlightedIndex
            model: control.popup.visible ? control.delegateModel : null

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: palette.mid

            border.color: palette.dark
            border.width: 1
            radius: GlobalVars.standardRounding
        }
    }
}

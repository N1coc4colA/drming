import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AbstractButton {
    id: root

    implicitWidth: 160
    implicitHeight: 180
    padding: 16
    hoverEnabled: true

    property string accessibleDescription: ""

    display: AbstractButton.TextUnderIcon

    background: Rectangle {
        radius: 16
        color: root.down ? palette.highlight : palette.button
        border.color: root.activeFocus ? palette.highlight : palette.mid
        border.width: root.activeFocus ? 2 : 1
    }

    contentItem: ColumnLayout {
        spacing: 12

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 96

            Image {
                anchors.centerIn: parent
                source: root.iconSource
                sourceSize.width: 64
                sourceSize.height: 64
                fillMode: Image.PreserveAspectFit
                visible: source !== ""
                mipmap: true
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.text
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
    }

    focusPolicy: Qt.StrongFocus

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            clicked()
            event.accepted = true
        }
    }
}

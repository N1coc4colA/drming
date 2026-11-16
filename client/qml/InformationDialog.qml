import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

EasyDialog {
    id: root
    modal: true
    title: qsTr("Information")

    content: ColumnLayout {
        id: contentItem
        Layout.margins: GlobalVars.outterSpacing
        spacing: GlobalVars.standardSpacing

        Label {
            text: qsTr("Made by n1coc4cola.")
        }
        Label {
            text: qsTr("Icons credit: KDE Breeze Dark.")
        }
        Label {
            text: qsTr("Built with Qt, FFMPEG, and Android NDK depending on the platform.")
        }
    }

    footer: RowLayout {
        Layout.margins: GlobalVars.outterSpacing
        spacing: GlobalVars.outterSpacing

        EasyButton {
            icon.source: "qrc:/assets/window-close.svg"
            text: qsTr("Close")

            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole

            onClicked: {
                root.close()
                root.cancelled()
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}

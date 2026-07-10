import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

EasyDialog {
    id: root
    modal: true
    title: qsTr("Login")

    property string hostIp: ""
    property string hostPort: ""
    property string clientName: ""
    property bool isValid: false

    signal cancelled
    signal submitted

    content: ColumnLayout {
        id: contentItem
        Layout.margins: GlobalVars.outterSpacing
        spacing: GlobalVars.standardSpacing

        GridLayout {
            columns: 2

            Label {
                text: qsTr("Server IP:")
            }
            Label {
                text: root.hostIp
            }

            Label {
                text: qsTr("Server port:")
            }
            Label {
                text: root.hostPort
            }

            Label {
                text: qsTr("Key to use:")
            }
            EasyComboBox {
                id: clientEntry

                onCurrentIndexChanged: root.isValid = clientEntry.currentIndex > -1
                onCurrentTextChanged: root.clientName = currentText
                onVisibleChanged: clientEntry.model = fileProvider().validClientEntries()
            }
        }
    }

    footer: RowLayout {
        Layout.margins: GlobalVars.outterSpacing
        spacing: GlobalVars.outterSpacing

        EasyButton {
            icon.source: "qrc:/assets/window-close.svg"
            text: qsTr("Cancel")

            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole

            onClicked: {
                root.close()
                root.cancelled()
            }
        }
        Item {
            Layout.fillWidth: true
        }

        EasyButton {
            enabled: root.isValid
            icon.source: "qrc:/assets/go-next.svg"
            text: qsTr("Continue")

            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole

            onClicked: {
                root.close()
                root.submitted()
            }
        }
    }
}

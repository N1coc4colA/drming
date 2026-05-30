import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

EasyDialog {
    id: root
    modal: true
    title: qsTr("Login")

    property string hostIp: "";
    property string hostPort: "";
    property string clientName: "";
    property bool isValid: true

    signal submitted
    signal cancelled

    content: ColumnLayout {
        spacing: 8
        Layout.margins: 8

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

                onCurrentIndexChanged: {
                    root.isValid = currentIndex >= 0
                    root.clientName = currentText
                }
                onVisibleChanged: clientEntry.model = fileProvider.validClientEntries()
            }
        }
    }

    footer: RowLayout {
        spacing: 10

        Layout.margins: 8

        EasyButton {
            text: qsTr("Cancel")
            icon.source: "qrc:/assets/window-close.svg"

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
            text: qsTr("Continue")
            enabled: root.isValid
            icon.source: "qrc:/assets/go-next.svg"

            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole

            onClicked: {
                root.close()
                root.submitted()
            }
        }
    }
}

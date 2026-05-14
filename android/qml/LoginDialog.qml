import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("Login")
    anchors.centerIn: ApplicationWindow.overlay

    property string hostIp: "";
    property string hostPort: "";
    property bool isValid: true//clientPinField.acceptableInput && serverPinField.acceptableInput

    //property alias pinCode: clientPinField.text
    //property alias serverPinCode: serverPinField.text
    property int pinCode: 0
    property int serverPinCode: 0

    signal submitted
    signal cancelled

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

        /*Label {
            text: qsTr("Server PIN")
        }
        TextField {
            id: serverPinField
            placeholderText: "PIN"
            maximumLength: 4
            inputMask: "9999"
            inputMethodHints: Qt.ImhDigitsOnly
            echoMode: TextInput.Password
        }

        Label {
            text: qsTr("Client PIN")
        }
        TextField {
            id: clientPinField
            placeholderText: "PIN"
            maximumLength: 4
            inputMask: "9999"
            inputMethodHints: Qt.ImhDigitsOnly
            echoMode: TextInput.Password
        }*/
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
        Button {
            text: qsTr("Continue")
            enabled: root.isValid
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: root.submitted()
        }
    }

    onAccepted: function() {
        root.close();
        root.submitted();
    }

    onRejected: function() {
        root.close();
        root.cancelled();
    }
}

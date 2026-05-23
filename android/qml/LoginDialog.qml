import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("Login")
    modal: true

    property string hostIp: "";
    property string hostPort: "";
    property bool isValid: true

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

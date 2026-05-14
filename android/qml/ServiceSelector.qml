import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: popup
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 400)
    height: contentHeight + 40
    modal: true
    focus: true

    signal accepted
    signal rejected

    function open(service) {
        serviceInfo = service
        popup.open()
    }

    property var serviceInfo: null

    ColumnLayout {
        width: parent.width - 40
        anchors.centerIn: parent
        spacing: 16

        Text {
            text: qsTr("Connect to Service?")
            font.pixelSize: 20
            font.bold: true
            color: "#333333"
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#e0e0e0"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("Service: %1").arg(popup.serviceInfo ? name : "")
                font.pixelSize: 14
                color: "#333333"
            }

            Text {
                text: qsTr("Host: %1").arg(popup.serviceInfo ? host : "")
                font.pixelSize: 14
                color: "#666666"
            }

            Text {
                text: qsTr("Port: %1").arg(popup.serviceInfo ? port : "")
                font.pixelSize: 14
                color: "#666666"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#e0e0e0"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                onClicked: {
                    popup.rejected()
                    popup.close()
                }
            }

            Button {
                text: qsTr("Accept")
                Layout.fillWidth: true
                onClicked: {
                    popup.accepted()
                    popup.close()
                }
            }
        }
    }
}

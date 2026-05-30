import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: popup
    focus: true
    modal: true
    title: qsTr("Connect to Service?")
    width: Math.min(parent.width - 40, 400)
    height: contentHeight + 40

    anchors.centerIn: parent

    property var serviceInfo: null

    signal accepted
    signal rejected

    function open(service) {
        serviceInfo = service
        popup.open()
    }

    ColumnLayout {
        spacing: 16
        width: parent.width - 40

        anchors.centerIn: parent

        Rectangle {
            height: 1

            Layout.fillWidth: true
        }

        ColumnLayout {
            spacing: 8

            Layout.fillWidth: true

            Text {
                text: qsTr("Service: %1").arg(popup.serviceInfo ? name : "")
                font.pixelSize: 14
            }

            Text {
                text: qsTr("Host: %1").arg(popup.serviceInfo ? host : "")
                font.pixelSize: 14
            }

            Text {
                text: qsTr("Port: %1").arg(popup.serviceInfo ? port : "")
                font.pixelSize: 14
            }
        }
    }
}

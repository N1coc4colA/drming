import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: popup
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 400)
    height: contentHeight + 40
    modal: true
    focus: true
    title: qsTr("Connect to Service?")

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

        Rectangle {
            Layout.fillWidth: true
            height: 1
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

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

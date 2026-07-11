import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: popup
    focus: true
    modal: true
    title: qsTr("Connect to Service?")

    anchors.centerIn: parent
    height: contentHeight + 40
    width: Math.min(parent.width - 40, 400)

    property var serviceInfo: null

    signal accepted
    signal rejected

    function open(service) {
        serviceInfo = service
        popup.open()
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        width: parent.width - 40

        Rectangle {
            height: 1
            Layout.fillWidth: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: GlobalVars.standardSpacing

            Text {
                font.pixelSize: GlobalVars.fontSizeMedium
                text: qsTr("Service: %1").arg(popup.serviceInfo ? name : "")
            }

            Text {
                font.pixelSize: GlobalVars.fontSizeMedium
                text: qsTr("Host: %1").arg(popup.serviceInfo ? host : "")
            }

            Text {
                font.pixelSize: GlobalVars.fontSizeMedium
                text: qsTr("Port: %1").arg(popup.serviceInfo ? port : "")
            }
        }
    }
}

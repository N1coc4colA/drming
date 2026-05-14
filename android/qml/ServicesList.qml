import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: servicesList
    color: "transparent"

    property string searchQuery: ""
    property var selectedService: ({
        name: "",
        host: "",
        ip: "",
        type: "",
        port: 0,
    })

    signal serviceSelected(var service)

    ListView {
        id: listView
        anchors.fill: parent
        spacing: 8
        clip: true
        model: servicesModel  // Use the C++ model directly

        delegate: Rectangle {
            width: listView.width
            height: visible ? 140 : 0
            color: "#ffffff"
            radius: 8
            border.color: "#e0e0e0"
            border.width: 1

            // Filter based on search query
            visible: model.name.toLowerCase().includes(searchQuery.toLowerCase())

            MouseArea {
                anchors.fill: parent
                onClicked: function() {
                    selectedService.name = model.name;
                    selectedService.host = model.host;
                    selectedService.ip = model.ip;
                    selectedService.port = model.port;
                    selectedService.type = model.type;

                    servicesList.serviceSelected(selectedService);
                }

                ColumnLayout {
                    id: cl
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 4

                    Text {
                        text: model.name
                        font.pixelSize: 18
                        font.bold: true
                        color: "#333333"
                    }

                    Text {
                        text: qsTr("Host: %1").arg(model.host)
                        font.pixelSize: 14
                        color: "#666666"
                        Layout.maximumWidth: 100;
                    }

                    Text {
                        text: qsTr("IP: %1").arg(model.ip)
                        font.pixelSize: 14
                        color: "#666666"
                        Layout.maximumWidth: 100;
                    }

                    Text {
                        text: qsTr("Port: %1").arg(model.port)
                        font.pixelSize: 14
                        color: "#666666"
                        Layout.maximumWidth: 100;
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            active: servicesModel.count > 0
        }
    }

    Text {
        anchors.centerIn: parent
        text: networkState.connected ?  qsTr("No services found") : qsTr("No internet connection")
        font.pixelSize: 16
        color: "#999999"
        visible: servicesModel.count === 0
    }
}

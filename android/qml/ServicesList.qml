import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

EasyListView {
    id: root
    emptyText: networkState.connected ?  qsTr("No services found") : qsTr("No internet connection")
    model: servicesModel

    property string searchQuery: ""
    property var selectedService: ({
        name: "",
        host: "",
        ip: "",
        type: "",
        port: 0,
    })

    signal serviceSelected(var service)

    delegate: Item {
        width: root.width
        height: visible ? 140 : 0
        visible: model.name.toLowerCase().includes(searchQuery.toLowerCase())

        Rectangle {
            radius: 8
            color: palette.mid

            border.color: palette.dark
            border.width: 1

            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: anchors.leftMargin

            MouseArea {
                anchors.fill: parent

                ColumnLayout {
                    id: cl
                    spacing: 4

                    anchors.fill: parent
                    anchors.margins: 16

                    Label {
                        text: model.name
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Label {
                        text: qsTr("Host: %1").arg(model.host)
                        font.pixelSize: 14

                        Layout.maximumWidth: 100;
                    }

                    Label {
                        text: qsTr("IP: %1").arg(model.ip)
                        font.pixelSize: 14

                        Layout.maximumWidth: 100;
                    }

                    Label {
                        text: qsTr("Port: %1").arg(model.port)
                        font.pixelSize: 14

                        Layout.maximumWidth: 100;
                    }
                }

                onClicked: function() {
                    selectedService.name = model.name
                    selectedService.host = model.host
                    selectedService.ip = model.ip
                    selectedService.port = model.port
                    selectedService.type = model.type

                    root.serviceSelected(selectedService)
                }
            }
        }
    }
}

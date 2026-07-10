import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

EasyListView {
    id: root
    emptyText: networkState().connected ?  qsTr("No services found") : qsTr("No internet connection")
    model: servicesModel()

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
        visible: model.name.toLowerCase().includes(searchQuery.toLowerCase())

        height: visible ? 140 : 0
        width: root.width

        Rectangle {
            color: palette.mid

            border.color: palette.dark
            border.width: 1
            radius: GlobalVars.standardRounding
            anchors.fill: parent
            anchors.leftMargin: GlobalVars.innerSpacing
            anchors.rightMargin: anchors.leftMargin

            MouseArea {
                anchors.fill: parent

                ColumnLayout {
                    id: cl

                    anchors.fill: parent
                    anchors.margins: GlobalVars.standardSpacing*2
                    spacing: GlobalVars.standardSpacing/2

                    Label {
                        font.pixelSize: Math.max(18, 18 * GlobalVars.scaling)
                        font.bold: true
                        text: model.name
                    }

                    Label {
                        font.pixelSize: Math.max(14, 14 * GlobalVars.scaling)
                        text: qsTr("Host: %1").arg(model.host)

                        Layout.maximumWidth: 100;
                    }

                    Label {
                        font.pixelSize: Math.max(14, 14 * GlobalVars.scaling)
                        text: qsTr("IP: %1").arg(model.ip)

                        Layout.maximumWidth: 100;
                    }

                    Label {
                        font.pixelSize: Math.max(14, 14 * GlobalVars.scaling)
                        text: qsTr("Port: %1").arg(model.port)

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

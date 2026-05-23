import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

Item {
    id: root

    property real topMargin: 10
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
        model: servicesModel

        ScrollBar.vertical: ScrollBar {
            id: scrollBar
            active: servicesModel.count > 0
            background: Rectangle {
                color: palette.dark
                opacity: scrollBar.contentItem.opacity
            }
        }

        header: Item {
            id: topSpacer
            height: topMargin * 1.5
            width: listView.width
        }

        footer: Item {
            height: listView.spacing
            width: listView.width
        }

        delegate: Item {
            width: listView.width
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
                    onClicked: function() {
                        selectedService.name = model.name;
                        selectedService.host = model.host;
                        selectedService.ip = model.ip;
                        selectedService.port = model.port;
                        selectedService.type = model.type;

                        root.serviceSelected(selectedService);
                    }

                    ColumnLayout {
                        id: cl
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 4

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
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        height: topMargin + listView.spacing
        gradient: Gradient {
            GradientStop { position: 0.0; color: palette.window }
            GradientStop { position: 0.5; color: palette.window }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Label {
        anchors.centerIn: parent
        text: networkState.connected ?  qsTr("No services found") : qsTr("No internet connection")
        font.pixelSize: 16
        visible: servicesModel.count === 0
    }

    onVisibleChanged: function(visibility) {
        if (visibility) {
            mdnsManager.startDiscovery();
        } else {
            mdnsManager.stopDiscovery();
        }
    }
}

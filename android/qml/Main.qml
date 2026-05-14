import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoStream

Window {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("Device Remote Manager")

    enum ViewState {
        ServiceList,
        Confirmed
    }

    property int viewState: Main.ViewState.ServiceList
    property var selectedService: null

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: servicesView

        Component {
            id: servicesView
            Rectangle {
                color: "#f5f5f5"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    SearchBar {
                        id: searchBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                    }

                    ServicesList {
                        id: servicesList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        searchQuery: searchBar.searchText

                        onServiceSelected: (service) => {
                            window.selectedService = service
                            authDialog.hostIp = service.ip;
                            authDialog.hostPort = service.port;
                            authDialog.open();
                        }
                    }
                }
            }
        }

        Component {
            id: streamViewerComponent
            VideoFrame {
                id: streamViewer

                Connections {
                    target: networkLink

                    function onImageReady(image) {
                        streamViewer.setImage(image);
                    }
                }
            }
        }
    }

    LoginDialog {
        id: authDialog

        onSubmitted: function() {
            networkLink.connect(authDialog.hostIp, authDialog.hostPort);
            stackView.replace(streamViewerComponent);
        }
    }

    Component.onCompleted: {
        mdnsManager.startDiscovery()
    }
}

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
                            window.selectedService = service;
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
        x: (window.width - width)/2
        y: (window.height - height)/2

        onSubmitted: function() {
            networkLink.connect(authDialog.hostIp, authDialog.hostPort);
            stackView.push(streamViewerComponent);
        }
    }

    Dialog {
        id: errorDialog
        x: (window.width - width)/2
        y: (window.height - height)/2
        title: qsTr("Connection error")
        modal: true
        standardButtons: Dialog.Ok
        onAccepted: {
            stackView.push(servicesView);
        }

        Label {
            id: errorLabel
            text: ""
            wrapMode: Text.Wrap
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: 16
        }
    }

    Connections {
        target: networkLink

        function onError(message) {
            // Show the message and return to the base services view
            errorLabel.text = message || qsTr("Unknown connection error");
            networkLink.close();
            stackView.pop();
            errorDialog.open();
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (window.isStreaming && stackView.depth > 1) {
                networkLink.close();
                window.isStreaming = false;
                stackView.pop();
                event.accepted = true;
            }
        }
    }

    Component.onCompleted: {
        mdnsManager.startDiscovery();
    }
}

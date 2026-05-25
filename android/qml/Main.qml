import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("Device Remote Manager")
    color: palette.window

    enum ViewState {
        HomePage,
        CertificatesPages,
        KeysPage,
        ServicesPage,
        Streaming
    }

    property int viewState: Main.ViewState.HomePage

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: homeView

        Component {
            id: homeView

            HomePage {
                id: hom

                onCertsViewNeeded: {
                    stackView.push(certsView)
                }
                onKeysViewNeeded: {
                    stackView.push(keysView)
                }
                onServicesViewNeeded: {
                    stackView.push(servicesView)
                }
            }
        }

        Component {
            id: certsView
            CertificatesPages {
                id: certificates
                visible: false

                onBack: goBack()
            }
        }

        Component {
            id: keysView
            KeysPage {
                id: keys
                visible: false

                onBack: goBack()
            }
        }

        Component {
            id: servicesView
            ServicesPage {
                id: services
                visible: false

                onBack: goBack()
                onDisplayStream: {
                    stackView.pop()
                    stackView.push(streamView)
                }
            }
        }

        Component {
            id: streamView
            StreamPage {
                id: stream
                visible: false

                onBack: goBack()
            }
        }
    }

    Dialog {
        id: errorDialog
        x: (window.width - width)/2
        y: (window.height - height)/2
        title: qsTr("Connection error")
        modal: true
        standardButtons: Dialog.Ok

        Label {
            id: errorLabel
            text: ""
            wrapMode: Text.Wrap
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

    function goBack() {
        if (stackView.depth > 1) {
            if (window.isStreaming) {
                networkLink.close();
                window.isStreaming = false;
            }

            stackView.pop();
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            goBack();
            event.accepted = true;
        }
    }
}

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

        Component {
            id: shaderBlurEffectSource
            ShaderEffectSource {
                sourceItem: stackView
                visible: false
                anchors.fill: shaderBlurEffectSource.parent
                hideSource: false
                live: true
                textureSize: Qt.size(width / 2, height / 2)
            }
        }

        Loader {
            id: blurLoader
            anchors.fill: parent
            sourceComponent: shaderBlurEffectSource
        }
    }

    // Keep the singleton's shaderBlurSource in sync with the Loader's instantiated item.
    Binding {
        target: GlobalVars
        property: "shaderBlurSource"
        value: blurLoader.item
    }

    EasyDialog {
        id: errorDialog
        x: (window.width - width)/2
        y: (window.height - height)/2
        title: qsTr("Connection error")
        modal: true
        property string errorText: ""

        content: Label {
            id: errorLabel
            text: errorDialog.errorText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.margins: 16
        }

        footer: RowLayout {
            spacing: 10
            Layout.margins: 8

            Item {
                Layout.fillWidth: true
            }
            EasyButton {
                text: qsTr("Ok")
                icon.source: "qrc:/assets/window-close.svg"
                DialogButtonBox.buttonRole: DialogButtonBox.Ok
                onClicked: {
                    errorDialog.close()
                }
            }
        }
    }

    Connections {
        target: networkLink

        function onError(message) {
            // Show the message and return to the base services view
            errorDialog.errorText = message || qsTr("Unknown connection error");
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

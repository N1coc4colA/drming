import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


Window {
    id: window
    color: palette.window
    width: 640
    height: 480
    title: qsTr("Device Remote Manager")
    visible: true

    enum ViewState {
        HomePage,
        CertificatesPages,
        KeysPage,
        ServicesPage,
        Streaming
    }

    property int viewState: Main.ViewState.HomePage

    function goBack() {
        if (stackView.depth > 1) {
            if (window.isStreaming) {
                networkLink.close()
                window.isStreaming = false
            }

            stackView.pop()
        }
    }

    StackView {
        id: stackView
        initialItem: homeView

        anchors.fill: parent

        Component {
            id: homeView

            HomePage {
                id: hom

                onCertsViewNeeded: stackView.push(certsView)
                onKeysViewNeeded: stackView.push(keysView)
                onServicesViewNeeded: stackView.push(servicesView)
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
                hideSource: false
                live: true
                textureSize: Qt.size(width / 2, height / 2)
                sourceItem: stackView
                visible: false

                anchors.fill: shaderBlurEffectSource.parent
            }
        }

        Loader {
            id: blurLoader
            sourceComponent: shaderBlurEffectSource

            anchors.fill: parent
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
        title: qsTr("Connection error")
        modal: true
        x: (window.width - width)/2
        y: (window.height - height)/2

        property string errorText: ""

        content: Label {
            id: errorLabel
            horizontalAlignment: Text.AlignHCenter
            text: errorDialog.errorText
            wrapMode: Text.Wrap

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

                onClicked: errorDialog.close()
            }
        }
    }

    Connections {
        target: networkLink

        function onError(message) {
            // Show the message and return to the base services view
            errorDialog.errorText = message || qsTr("Unknown connection error")
            networkLink.close()
            stackView.pop()
            errorDialog.open()
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            goBack()
            event.accepted = true
        }
    }
}

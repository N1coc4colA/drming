import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


Window {
    id: window
    color: palette.window
    title: qsTr("Device Remote Manager")
    visible: true

    width: 640
    height: 480

    function goBack() {
        if (stackView.depth > 1) {
            networkLink().close()
            stackView.pop()
        }
    }

    Binding {
        target: GlobalVars
        property: "screen"
        value: window.screen
    }

    StackView {
        id: stackView
        initialItem: homeLoader.item

        anchors.fill: parent

        Component {
            id: homeView

            HomePage {
                id: hom

                onCertsViewNeeded: {
                    certsLoader.active = true
                    stackView.push(certsLoader.item)
                }
                onKeysViewNeeded: {
                    keysLoader.active = true
                    stackView.push(keysLoader.item)
                }
                onServicesViewNeeded: {
                    servicesLoader.active = true
                    stackView.push(servicesLoader.item)
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
                    streamLoader.active = true
                    stackView.push(streamLoader.item)
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

        // Lazy-loaded page instances
        Loader {
            id: homeLoader
            sourceComponent: homeView
            active: true
        }

        Loader {
            id: certsLoader
            sourceComponent: certsView
            active: false
        }

        Loader {
            id: keysLoader
            sourceComponent: keysView
            active: false
        }

        Loader {
            id: servicesLoader
            sourceComponent: servicesView
            active: false
        }

        Loader {
            id: streamLoader
            sourceComponent: streamView
            active: false
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
        modal: true
        title: qsTr("Connection error")

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
            Layout.margins: GlobalVars.outterSpacing
            spacing: GlobalVars.outterSpacing

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
        target: networkLink()

        function onError(message) {
            // Show the message and return to the base services view
            errorDialog.errorText = message || qsTr("Unknown connection error")
            networkLink().close()
            stackView.pop()
            errorDialog.open()
        }
    }

    // [TODO] Could not attach Keys property to:  Main_QMLTYPE_0(0x1cf65ba0)  is not an Item
    Keys.onReleased: (event) => {
        if (event.key === Qt.Key_Back) {
            goBack()
            event.accepted = true
        }
    }
}

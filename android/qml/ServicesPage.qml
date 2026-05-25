import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    property var selectedService: null
    property string searchText: ""

    signal displayStream

    content: ServicesList {
        id: servicesList
        Layout.fillWidth: true
        Layout.fillHeight: true
        searchQuery: root.searchText

        onServiceSelected: (service) => {
            root.selectedService = service;
            authDialog.hostIp = service.ip;
            authDialog.hostPort = service.port;
            authDialog.open();
        }
    }

    ShaderEffectSource {
        id: shaderBlurSource
        sourceItem: root
        visible: false
        anchors.fill: parent
        hideSource: false
        live: true
        textureSize: Qt.size(width / 2, height / 2)
    }

    LoginDialog {
        id: authDialog
        blurSource: shaderBlurSource

        onSubmitted: function() {
            networkLink.connect(authDialog.hostIp, authDialog.hostPort);
            root.displayStream();
        }
    }

    Component.onCompleted: {
        mdnsManager.startDiscovery();
    }
}

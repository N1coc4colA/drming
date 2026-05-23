import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import VideoStream

Item {
    id: root

    property var selectedService: null

    signal displayStream

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SearchBar {
            id: searchBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.leftMargin: 5
            Layout.rightMargin: Layout.leftMargin
            Layout.topMargin: Layout.leftMargin
        }

        ServicesList {
            id: servicesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            searchQuery: searchBar.searchText

            onServiceSelected: (service) => {
                root.selectedService = service;
                authDialog.hostIp = service.ip;
                authDialog.hostPort = service.port;
                authDialog.open();
            }
        }
    }

    LoginDialog {
        id: authDialog
        x: (window.width - width)/2
        y: (window.height - height)/2

        onSubmitted: function() {
            networkLink.connect(authDialog.hostIp, authDialog.hostPort);
            root.displayStream();
        }
    }

    Component.onCompleted: {
        mdnsManager.startDiscovery();
    }
}

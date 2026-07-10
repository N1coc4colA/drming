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
        searchQuery: root.searchText

        Layout.fillWidth: true
        Layout.fillHeight: true

        onServiceSelected: (service) => {
            root.selectedService = service
            authDialog.hostIp = service.ip
            authDialog.hostPort = service.port
            authDialog.open()
        }
    }

    LoginDialog {
        id: authDialog

        onSubmitted: function() {
            networkLink().connect(authDialog.hostIp, authDialog.hostPort, authDialog.clientName)
            root.displayStream()
        }
    }

    onVisibleChanged: {
        if (root.visible) {
            mdnsManager().startDiscovery()
            fileProvider().loadClients()
        } else {
            mdnsManager().stopDiscovery()
        }
    }
}

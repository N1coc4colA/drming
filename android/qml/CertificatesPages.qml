import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    property real topMargin: 10
    property string searchQuery: ""

    headerBar.rightContent: NewButton {
        icon.source: "qrc:/assets/document-new.svg"
        height: headerBar.centerHeight
        width: headerBar.centerHeight

        onClicked: fileProvider.addServerCert()
    }

    content: EasyListView {
        emptyText: qsTr("No certificates loaded")
        model: fileProvider.serverCertsModel()

        delegate: RowLayout {
            width: root.width
            height: visible ? 40 : 0
            visible: model.name.toLowerCase().includes(searchQuery.toLowerCase())
            property real breakPoint: width > 500

            Item {
                Layout.fillWidth: breakPoint
            }

            Rectangle {
                radius: 8
                color: palette.mid
                border.color: palette.dark
                border.width: 1
                anchors.leftMargin: breakPoint ? 8 : 0

                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.width - 16
                Layout.maximumWidth: 500

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 4
                    spacing: 8

                    Label {
                        text: model.datetime
                        font.pixelSize: 14
                        color: "#fcd757"
                        elide: Text.ElideRight
                    }
                    Label {
                        id: label
                        text: model.name
                        font.pixelSize: 14
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    EasyButton {
                        icon.source: "qrc:/assets/edit-delete.svg"
                        display: AbstractButton.IconOnly
                        height: label.implicitHeight
                        width: label.implicitHeight
                        borderColor: "transparent"
                        backgroundColor: "transparent"
                        highlightColor: "#e33636"

                        onClicked: fileProvider.deleteServerCert(model.name)
                    }
                }
            }

            Item {
                Layout.fillWidth: breakPoint
            }
        }
    }

    onVisibleChanged: {
        if (root.visible) {
            fileProvider.loadServerCerts();
        }
    }
}

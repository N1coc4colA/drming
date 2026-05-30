import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    property real topMargin: 10

    headerBar.rightContent: NewButton {
        width: headerBar.centerHeight
        height: headerBar.centerHeight

        icon.source: "qrc:/assets/document-new.svg"

        onClicked: fileProvider.addServerCert()
    }

    content: EasyListView {
        emptyText: qsTr("No certificates loaded")
        model: fileProvider.serverCertsModel()

        delegate: RowLayout {
            width: root.width
            height: visible ? 40 : 0
            visible: model.name.toLowerCase().includes(root.searchText.toLowerCase())

            property real breakPoint: width > 500

            Item {
                Layout.fillWidth: breakPoint
            }

            Rectangle {
                border.color: palette.dark
                border.width: 1
                color: palette.mid
                radius: 8

                anchors.leftMargin: breakPoint ? 8 : 0
                Layout.maximumWidth: 500
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.width - 16

                RowLayout {
                    spacing: 8

                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 4

                    Label {
                        text: model.datetime
                        color: "#fcd757"
                        elide: Text.ElideRight

                        font.pixelSize: 14
                    }
                    Label {
                        id: label
                        text: model.name
                        elide: Text.ElideRight

                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                    EasyButton {
                        display: AbstractButton.IconOnly
                        width: label.implicitHeight
                        height: label.implicitHeight
                        borderColor: "transparent"
                        backgroundColor: "transparent"
                        highlightColor: "#e33636"

                        icon.source: "qrc:/assets/edit-delete.svg"

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
            fileProvider.loadServerCerts()
        }
    }
}

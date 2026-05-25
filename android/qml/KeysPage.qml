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

        onClicked: {}
    }

    content: EasyListView {
        emptyText: qsTr("No keys loaded")
        model: fileProvider.clientCertsModel()

        delegate: Item {
            width: root.width
            height: visible ? 40 : 0
            visible: model.name.toLowerCase().includes(searchQuery.toLowerCase())

            Rectangle {
                radius: 8
                color: palette.mid
                border.color: palette.dark
                border.width: 1
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: anchors.leftMargin

                RowLayout {
                    Label {
                        text: model.datetime
                        font.pixelSize: 14
                    }
                    Label {
                        text: model.name
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
}

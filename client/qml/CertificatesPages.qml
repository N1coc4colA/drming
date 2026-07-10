import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    headerBar.rightContent: NewButton {
        implicitHeight: headerBar.centerHeight
        implicitWidth: headerBar.centerHeight

        icon.source: "qrc:/assets/document-new.svg"

        onClicked: fileProvider().addServerCert()
    }

    content: EasyListView {
        emptyText: qsTr("No certificates loaded")
        model: fileProvider().serverCertsModel()

        delegate: RowLayout {
            height: visible ? Math.max(40, 40 * GlobalVars.scaling) : 0
            width: root.width

            visible: model.name.toLowerCase().includes(root.searchText.toLowerCase())

            property real breakPoint: width > Math.max(500, 500 * GlobalVars.scaling)

            Item {
                Layout.fillWidth: breakPoint
            }

            Rectangle {
                color: palette.mid

                anchors.leftMargin: breakPoint ? GlobalVars.standardSpacing : 0
                border.color: palette.dark
                border.width: 1
                radius: GlobalVars.standardRounding
                Layout.maximumWidth: Math.max(500, 500 * GlobalVars.scaling)
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.width - GlobalVars.standardSpacing*2

                RowLayout {
                    spacing: GlobalVars.standardSpacing
                    anchors.fill: parent
                    anchors.leftMargin: GlobalVars.standardSpacing*2
                    anchors.rightMargin: GlobalVars.standardSpacing/2

                    Label {
                        color: "#fcd757"
                        elide: Text.ElideRight
                        font.pixelSize: Math.max(14, 14 * GlobalVars.scaling)
                        text: model.datetime
                    }
                    Label {
                        id: label
                        elide: Text.ElideRight
                        font.pixelSize: Math.max(14, 14 * GlobalVars.scaling)
                        text: model.name

                        Layout.fillWidth: true
                    }
                    EasyButton {
                        display: AbstractButton.IconOnly
                        icon.source: "qrc:/assets/edit-delete.svg"

                        height: label.implicitHeight
                        width: label.implicitHeight

                        backgroundColor: "transparent"
                        borderColor: "transparent"
                        highlightColor: "#e33636"

                        onClicked: fileProvider().deleteServerCert(model.name)
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
            fileProvider().loadServerCerts()
        }
    }
}

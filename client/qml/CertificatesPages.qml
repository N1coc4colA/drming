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

            // Cache breakpoint calculation to avoid recalculation per frame
            property bool isWide: width > Math.max(500, 500 * GlobalVars.scaling)

            Item {
                Layout.fillWidth: isWide
            }

            Rectangle {
                color: palette.mid

                anchors.leftMargin: isWide ? GlobalVars.standardSpacing : 0
                border.color: palette.dark
                border.width: 1
                radius: GlobalVars.standardRounding
                Layout.maximumWidth: Math.max(500, 500 * GlobalVars.scaling)
                Layout.preferredHeight: parent.height
                Layout.preferredWidth: parent.width - GlobalVars.doubleStandardSpacing

                RowLayout {
                    spacing: GlobalVars.standardSpacing
                    anchors.fill: parent
                    anchors.leftMargin: GlobalVars.doubleStandardSpacing
                    anchors.rightMargin: GlobalVars.halfStandardSpacing

                    Label {
                        color: "#fcd757"
                        elide: Text.ElideRight
                        font.pixelSize: GlobalVars.fontSizeMedium
                        text: model.datetime
                    }
                    Label {
                        id: label
                        elide: Text.ElideRight
                        font.pixelSize: GlobalVars.fontSizeMedium
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
                Layout.fillWidth: isWide
            }
        }
    }

    onVisibleChanged: {
        if (root.visible) {
            fileProvider().loadServerCerts()
        }
    }
}

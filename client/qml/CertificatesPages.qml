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

        delegate: Item {
            id: delegateItem
            height: visible ? Math.max(40, GlobalVars.rowHeightDefault) : 0
            width: root.width
            visible: model.name.toLowerCase().includes(root.searchText.toLowerCase())

            property bool isWide: width > GlobalVars.wideBreakpoint

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: isWide ? GlobalVars.doubleStandardSpacing : GlobalVars.standardSpacing
                anchors.rightMargin: isWide ? GlobalVars.doubleStandardSpacing : GlobalVars.standardSpacing
                spacing: 0

                Item { Layout.fillWidth: true; visible: delegateItem.isWide }

                Rectangle {
                    color: palette.mid
                    border.color: palette.dark
                    border.width: 1
                    radius: GlobalVars.standardRounding

                    Layout.fillHeight: true
                    Layout.maximumWidth: GlobalVars.wideBreakpoint
                    Layout.fillWidth: true

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
                            icon.source: "qrc:/assets/document-save.svg"
                            implicitHeight: label.implicitHeight + 8
                            implicitWidth: label.implicitHeight + 8
                            backgroundColor: "transparent"
                            borderColor: "transparent"
                            onClicked: fileProvider().exportServerCert(model.name)
                        }
                        EasyButton {
                            display: AbstractButton.IconOnly
                            icon.source: "qrc:/assets/edit-delete.svg"
                            implicitHeight: label.implicitHeight + 8
                            implicitWidth: label.implicitHeight + 8
                            backgroundColor: "transparent"
                            borderColor: "transparent"
                            highlightColor: "#e33636"
                            onClicked: fileProvider().deleteServerCert(model.name)
                        }
                    }
                }

                Item { Layout.fillWidth: true; visible: delegateItem.isWide }
            }
        }
    }

    onVisibleChanged: {
        if (root.visible) {
            fileProvider().loadServerCerts()
        }
    }
}

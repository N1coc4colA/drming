import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    property real topMargin: 10

    EasyDialog {
        id: infoDialog
        x: (parent.width - width)/2
        y: (parent.height - height)/2
        title: qsTr("Client data")
        modal: true

        width: root.width > 260 ? 250 : root.width - 10

        property string errorText: ""
        property var model: QtObject {
            property string entryName: ""
            property string previousName: ""
            property var info: QtObject {
                property bool key: false
                property bool cert: false
            }
        }

        function generateMap() {
            return {
                "previousName": infoDialog.model.previousName,
                "updatedName": infoDialog.model.entryName,
            }
        }

        function updateClientData() {
            if (fileProvider.updateClientEntry(generateMap())) {
                infoDialog.model.previousName = infoDialog.model.entryName
            }
        }

        content: GridLayout {
            columns: root.width > 260 ? 2 : 1

            Label {
                text: qsTr("Name")
            }
            Rectangle {
                radius: 5
                color: palette.mid
                Layout.fillWidth: true
                height: nameInput.implicitHeight + 10

                TextInput {
                    id: nameInput
                    anchors.fill: parent
                    anchors.margins: 3
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    text: infoDialog.model.entryName
                    color: palette.text
                    verticalAlignment: TextInput.AlignVCenter

                    onTextEdited: {
                        infoDialog.model.entryName = nameInput.text
                        infoDialog.updateClientData()
                    }

                    onEditingFinished: {
                        infoDialog.model.entryName = nameInput.text
                        infoDialog.updateClientData()
                    }
                }
            }

            Label {
                text: qsTr("Key")
                visible: nameInput.length !== 0
            }
            EasyButton {
                text: qsTr("Change")
                visible: nameInput.length !== 0
                onClicked: {
                    const output = fileProvider.addClientKey(infoDialog.model.entryName)
                    switch (output) {
                    case 0: {
                        errorLabel.text = qsTr("Failed to open key file.")
                        break;
                    }
                    case 1: {
                        errorLabel.text = qsTr("Invalid key file.")
                        break;
                    }
                    case 2: {
                        infoDialog.model.info.key = true
                    }
                    }
                }
                icon.source: infoDialog.model.info.key ? "qrc:/assets/check.svg" : "qrc:/assets/warning-red.svg"
                color: infoDialog.model.info.key ? "#17c245" : "#ff3045"
            }

            Label {
                text: qsTr("Certificate")
                visible: nameInput.length !== 0
            }

            EasyButton {
                text: qsTr("Change")
                visible: nameInput.length !== 0
                onClicked: {
                    const output = fileProvider.addClientCert(infoDialog.model.entryName)
                    switch (output) {
                    case 0: {
                        errorLabel.text = qsTr("Failed to open certificate file.")
                        break;
                    }
                    case 1: {
                        errorLabel.text = qsTr("Invalid certificate file.")
                        infoDialog.model.info.cert = false
                        break;
                    }
                    case 2: {
                        infoDialog.model.info.cert = true
                    }
                    }
                }
                icon.source: infoDialog.model.info.cert ? "qrc:/assets/check.svg" : "qrc:/assets/warning-red.svg"
                color: infoDialog.model.info.cert ? "#17c245" : "#ff3045"
            }

            Label {
                id: errorLabel
                visible: errorLabel.text.length !== 0
                color: "#ff3045"
                Layout.fillWidth: true
                Layout.columnSpan: parent.columns
                horizontalAlignment: Text.AlignHCenter
            }
        }

        footer: RowLayout {
            spacing: 10
            Layout.margins: 8

            Item {
                Layout.fillWidth: true
            }
            EasyButton {
                text: qsTr("Close")
                icon.source: "qrc:/assets/window-close.svg"
                DialogButtonBox.buttonRole: DialogButtonBox.Ok
                onClicked: {
                    infoDialog.close()
                }
            }
        }
    }

    headerBar.rightContent: NewButton {
        icon.source: "qrc:/assets/document-new.svg"
        height: headerBar.centerHeight
        width: headerBar.centerHeight

        onClicked: {
            infoDialog.model.entryName = ""
            infoDialog.model.previousName = ""
            infoDialog.model.info.key = false
            infoDialog.model.info.cert = false
            infoDialog.open()
        }
    }

    content: EasyListView {
        emptyText: qsTr("No keys loaded")
        model: fileProvider.clientCertsModel()

        delegate: RowLayout {
            width: root.width
            height: visible ? 40 : 0
            visible: model.name.toLowerCase().includes(root.searchText.toLowerCase())
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

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        infoDialog.model.entryName = model.name
                        infoDialog.model.previousName = model.name
                        infoDialog.model.info.key = model.info.key
                        infoDialog.model.info.cert = model.info.cert
                        infoDialog.open()
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 4
                    spacing: 8

                    Image {
                        width: label.height
                        height: label.height
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: label.height
                        sourceSize.height: label.height
                        source: (model.info["cert"] && model.info["key"]) ? "qrc:/assets/check.svg" : "qrc:/assets/warning.svg"
                    }

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

                        onClicked: fileProvider.deleteClient(model.name)
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
            fileProvider.loadClients();
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StandardPage {
    id: root

    property real dialogMaxWidth: Math.max(250, 250 * GlobalVars.scaling)
    property real dialogMinMargin: GlobalVars.spacingBase

    EasyDialog {
        id: infoDialog
        modal: true
        title: qsTr("Client data")

        x: (parent.width - width)/2
        y: (parent.height - height)/2
        width: root.width > Math.max(260, 260 * GlobalVars.scaling) ? root.dialogMaxWidth : root.width - root.dialogMinMargin

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
            if (fileProvider().updateClientEntry(generateMap())) {
                infoDialog.model.previousName = infoDialog.model.entryName
            }
        }

        content: GridLayout {
            columns: root.width > Math.max(260, 260 * GlobalVars.scaling) ? 2 : 1
            columnSpacing: GlobalVars.spacingSmall
            rowSpacing: GlobalVars.spacingSmall

            Label {
                font.pixelSize: GlobalVars.fontSizeXSmall
                text: qsTr("Name")
            }
            Rectangle {
                color: palette.mid

                height: nameInput.implicitHeight + GlobalVars.outterSpacing
                Layout.fillWidth: true
                radius: GlobalVars.innerRounding

                TextInput {
                    id: nameInput
                    color: palette.text
                    font.pixelSize: GlobalVars.fontSizeMedium
                    text: infoDialog.model.entryName
                    verticalAlignment: TextInput.AlignVCenter

                    anchors.fill: parent
                    anchors.margins: GlobalVars.spacingXSmall
                    anchors.leftMargin: GlobalVars.standardSpacing
                    anchors.rightMargin: GlobalVars.standardSpacing

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
                font.pixelSize: GlobalVars.fontSizeXSmall
                text: qsTr("Key")

                visible: nameInput.length !== 0
            }
            EasyButton {
                color: infoDialog.model.info.key ? "#17c245" : "#ff3045"
                icon.source: infoDialog.model.info.key ? "qrc:/assets/check.svg" : "qrc:/assets/warning-red.svg"
                font.pixelSize: GlobalVars.fontSizeXSmall
                text: qsTr("Change")

                visible: nameInput.length !== 0

                onClicked: {
                    errorLabel.text = ""
                    const output = fileProvider().addClientKey(infoDialog.model.entryName)
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
            }

            Label {
                font.pixelSize: GlobalVars.fontSizeXSmall
                text: qsTr("Certificate")

                visible: nameInput.length !== 0
            }

            EasyButton {
                color: infoDialog.model.info.cert ? "#17c245" : "#ff3045"
                font.pixelSize: GlobalVars.fontSizeXSmall
                icon.source: infoDialog.model.info.cert ? "qrc:/assets/check.svg" : "qrc:/assets/warning-red.svg"
                text: qsTr("Change")

                visible: nameInput.length !== 0

                onClicked: {
                    errorLabel.text = ""
                    const output = fileProvider().addClientCert(infoDialog.model.entryName)
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
            }

            Label {
                id: errorLabel
                color: "#ff3045"
                font.pixelSize: GlobalVars.fontSizeXSmall
                horizontalAlignment: Text.AlignHCenter

                visible: errorLabel.text.length !== 0

                Layout.fillWidth: true
                Layout.columnSpan: parent.columns
            }
        }

        footer: RowLayout {
            spacing: GlobalVars.outterSpacing
            Layout.margins: GlobalVars.outterSpacing

            Item {
                Layout.fillWidth: true
            }
            EasyButton {
                font.pixelSize: GlobalVars.fontSizeXSmall
                icon.source: "qrc:/assets/window-close.svg"
                text: qsTr("Close")

                DialogButtonBox.buttonRole: DialogButtonBox.Ok

                onClicked: {
                    infoDialog.close()
                }
            }
        }
    }

    headerBar.rightContent: NewButton {
        icon.source: "qrc:/assets/document-new.svg"

        implicitHeight: headerBar.centerHeight
        implicitWidth: headerBar.centerHeight

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
        model: fileProvider().clientCertsModel()

        delegate: RowLayout {
            height: visible ? GlobalVars.rowHeightDefault : 0
            width: root.width

            visible: model.name.toLowerCase().includes(root.searchText.toLowerCase())

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
                    anchors.leftMargin: GlobalVars.doubleStandardSpacing
                    anchors.rightMargin: GlobalVars.halfStandardSpacing
                    spacing: GlobalVars.standardSpacing

                    Image {
                        fillMode: Image.PreserveAspectFit
                        height: label.height
                        width: label.height

                        source: (model.info["cert"] && model.info["key"]) ? "qrc:/assets/check.svg" : "qrc:/assets/warning.svg"
                        sourceSize.width: label.height
                        sourceSize.height: label.height
                        cache: true
                    }

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

                        onClicked: fileProvider().deleteClient(model.name)
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
            fileProvider().loadClients();
        }
    }
}

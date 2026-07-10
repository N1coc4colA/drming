import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property int buttonsHeight: 120
    property int buttonsWidth: 120
    property int bounding: ((buttonsWidth < buttonsHeight) ? buttonsWidth : buttonsHeight) - 40
    property bool largeEnough: root.width > (buttonsWidth * 3 + 90)

    signal keysViewNeeded
    signal certsViewNeeded
    signal servicesViewNeeded

    Item {
        id: container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: gridLayout.height + label.height + 10 + 20

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: GlobalVars.outterSpacing
            spacing: GlobalVars.outterSpacing

            Label {
                id: label
                font.bold: true
                font.pixelSize: Math.max(16, 16 * GlobalVars.scaling)
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Welcome !")

                Layout.fillWidth: true
            }

            // Responsive grid: 3 columns when wide, 1 column when narrow
            GridLayout {
                id: gridLayout
                columns: 3

                Layout.fillWidth: true

                Item {
                    Layout.fillHeight: false
                    Layout.fillWidth: !largeEnough
                }
                GridLayout {
                    columns: largeEnough ? 7 : 1
                    columnSpacing: GlobalVars.outterSpacing
                    rowSpacing: GlobalVars.outterSpacing

                    Layout.fillWidth: largeEnough

                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                    }
                    EasyButton {
                        bounding: root.bounding
                        display: AbstractButton.TextUnderIcon
                        icon.source: "qrc:/assets/user-desktop.svg"
                        text: qsTr("Connect")

                        Layout.fillHeight: false
                        Layout.maximumHeight: buttonsHeight
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth

                        onClicked: root.servicesViewNeeded()
                    }
                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                        Layout.maximumWidth: 60
                    }
                    EasyButton {
                        bounding: root.bounding
                        display: AbstractButton.TextUnderIcon
                        icon.source: "qrc:/assets/application-certificate.svg"
                        text: qsTr("Certificates")

                        Layout.fillHeight: false
                        Layout.maximumHeight: buttonsHeight
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth

                        onClicked: root.certsViewNeeded()
                    }
                    Item {
                        Layout.maximumWidth: 60
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                    }
                    EasyButton {
                        id: btn
                        bounding: root.bounding
                        display: AbstractButton.TextUnderIcon
                        icon.source: "qrc:/assets/application-x-pem-key.svg"
                        text: qsTr("Keys")

                        Layout.fillHeight: false
                        Layout.maximumHeight: buttonsHeight
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth

                        onClicked: root.keysViewNeeded()
                    }
                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                    }
                }
                Item {
                    Layout.fillHeight: false
                    Layout.fillWidth: !largeEnough
                }
            }
        }
    }

    property alias mimumHeight: container.height
}

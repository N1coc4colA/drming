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
        height: gridLayout.height + label.height + 10 + 20

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        ColumnLayout {
            spacing: 10

            anchors.fill: parent
            anchors.margins: 10

            Label {
                id: label
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Welcome !")
                font.pixelSize: 16
                font.bold: true

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
                    rowSpacing: 10
                    columnSpacing: 10

                    Layout.fillWidth: largeEnough

                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                    }
                    EasyButton {
                        bounding: root.bounding
                        display: AbstractButton.TextUnderIcon
                        text: qsTr("Connect")
                        icon.source: "qrc:/assets/user-desktop.svg"

                        Layout.fillHeight: false
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight

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
                        text: qsTr("Certificates")
                        icon.source: "qrc:/assets/application-certificate.svg"

                        Layout.fillHeight: false
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight

                        onClicked: root.certsViewNeeded()
                    }
                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                        Layout.maximumWidth: 60
                    }
                    EasyButton {
                        id: btn
                        bounding: root.bounding
                        display: AbstractButton.TextUnderIcon
                        text: qsTr("Keys")
                        icon.source: "qrc:/assets/application-x-pem-key.svg"

                        Layout.fillHeight: false
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight

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

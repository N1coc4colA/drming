import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal keysViewNeeded
    signal certsViewNeeded
    signal servicesViewNeeded

    property int buttonsHeight: 120
    property int buttonsWidth: 120
    property int bounding: ((buttonsWidth < buttonsHeight) ? buttonsWidth : buttonsHeight) - 40
    property bool largeEnough: root.width > (buttonsWidth * 3 + 90)

    property alias mimumHeight: container.height

    Item {
        id: container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: gridLayout.height + label.height + 10 + 20

        ColumnLayout {
            spacing: 10
            anchors.fill: parent
            anchors.margins: 10

            Label {
                id: label
                text: qsTr("Welcome !")
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            // Responsive grid: 3 columns when wide, 1 column when narrow
            GridLayout {
                id: gridLayout
                Layout.fillWidth: true
                columns: 3

                Item {
                    Layout.fillHeight: false
                    Layout.fillWidth: !largeEnough
                }
                GridLayout {
                    Layout.fillWidth: largeEnough
                    columns: largeEnough ? 7 : 1
                    rowSpacing: 10
                    columnSpacing: 10

                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                    }
                    EasyButton {
                        display: AbstractButton.TextUnderIcon
                        text: qsTr("Connect")
                        icon.source: "qrc:/assets/user-desktop.svg"
                        bounding: root.bounding
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight
                        Layout.fillHeight: false
                        onClicked: root.servicesViewNeeded()
                    }
                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                        Layout.maximumWidth: 60
                    }
                    EasyButton {
                        display: AbstractButton.TextUnderIcon
                        text: qsTr("Certificates")
                        icon.source: "qrc:/assets/application-certificate.svg"
                        bounding: root.bounding
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight
                        Layout.fillHeight: false
                        onClicked: root.certsViewNeeded()
                    }
                    Item {
                        Layout.fillHeight: false
                        Layout.fillWidth: true
                        Layout.maximumWidth: 60
                    }
                    EasyButton {
                        id: btn
                        display: AbstractButton.TextUnderIcon
                        text: qsTr("Keys")
                        icon.source: "qrc:/assets/application-x-pem-key.svg"
                        bounding: root.bounding
                        Layout.preferredHeight: buttonsHeight
                        Layout.preferredWidth: buttonsWidth
                        Layout.maximumHeight: buttonsHeight
                        Layout.fillHeight: false
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
}

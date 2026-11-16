import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property int buttonsHeight: 120
    property int buttonsWidth: 120
    property int bounding: ((buttonsWidth < buttonsHeight) ? buttonsWidth : buttonsHeight) - 40

    signal keysViewNeeded
    signal certsViewNeeded
    signal servicesViewNeeded

    // Check if screen is wide enough to fit 3 buttons side-by-side cleanly
    readonly property bool isWideEnough: root.width > (buttonsWidth * 3 + GlobalVars.outterSpacing * 6)

    ColumnLayout {
        id: container
        anchors.centerIn: parent
        anchors.margins: GlobalVars.doubleOutterSpacing
        spacing: GlobalVars.outterSpacing * 2
        width: Math.min(parent.width - GlobalVars.doubleOutterSpacing * 2, implicitWidth)

        Label {
            id: label
            font.bold: true
            font.pixelSize: GlobalVars.fontSizeXLarge
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Welcome !")
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
        }

        // Responsive grid: 1 column on narrow portrait phones, 3 columns on landscape/tablets
        GridLayout {
            id: gridLayout
            Layout.alignment: Qt.AlignHCenter
            columns: root.isWideEnough ? 3 : 1
            rowSpacing: GlobalVars.outterSpacing * 2
            columnSpacing: GlobalVars.outterSpacing * 2

            EasyButton {
                bounding: root.bounding
                display: AbstractButton.TextUnderIcon
                icon.source: "qrc:/assets/user-desktop.svg"
                text: qsTr("Connect")
                implicitHeight: buttonsHeight
                implicitWidth: buttonsWidth
                Layout.preferredHeight: buttonsHeight
                Layout.preferredWidth: buttonsWidth
                Layout.alignment: Qt.AlignHCenter
                onClicked: root.servicesViewNeeded()
            }

            EasyButton {
                bounding: root.bounding
                display: AbstractButton.TextUnderIcon
                icon.source: "qrc:/assets/application-certificate.svg"
                text: qsTr("Certificates")
                implicitHeight: buttonsHeight
                implicitWidth: buttonsWidth
                Layout.preferredHeight: buttonsHeight
                Layout.preferredWidth: buttonsWidth
                Layout.alignment: Qt.AlignHCenter
                onClicked: root.certsViewNeeded()
            }

            EasyButton {
                id: btn
                bounding: root.bounding
                display: AbstractButton.TextUnderIcon
                icon.source: "qrc:/assets/application-x-pem-key.svg"
                text: qsTr("Keys")
                implicitHeight: buttonsHeight
                implicitWidth: buttonsWidth
                Layout.preferredHeight: buttonsHeight
                Layout.preferredWidth: buttonsWidth
                Layout.alignment: Qt.AlignHCenter
                onClicked: root.keysViewNeeded()
            }
        }
    }

    property alias mimumHeight: container.height
}

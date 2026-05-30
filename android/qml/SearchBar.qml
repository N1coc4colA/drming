import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar
    color: palette.mid
    radius: 8
    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    border.color: textInput.focus ? palette.highlight : palette.dark
    border.width: 1


    RowLayout {
        id: contentRow
        spacing: 5

        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        Image {
            fillMode: Image.PreserveAspectFit
            source: "qrc:/assets/edit-find.svg"
        }

        TextField {
            id: textInput
            placeholderText: qsTr("Search services...")
            font.pixelSize: 16

            background: Item {}

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Button {
            visible: textInput.text.length > 0
            icon.source: "qrc:/assets/edit-clear.svg"

            background: Item {}

            onClicked: textInput.text = ""
        }
    }

    property alias searchText: textInput.text
}

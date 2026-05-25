import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar
    radius: 8
    color: palette.mid
    border.color: textInput.focus ? palette.highlight : palette.dark
    border.width: 1

    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    property alias searchText: textInput.text

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        spacing: 5

        Image {
            fillMode: Image.PreserveAspectFit
            source: "qrc:/assets/edit-find.svg"
        }

        TextField {
            id: textInput
            Layout.fillWidth: true
            Layout.fillHeight: true
            placeholderText: qsTr("Search services...")
            background: Item {}
            font.pixelSize: 16
        }

        Button {
            icon.source: "qrc:/assets/edit-clear.svg"
            visible: textInput.text.length > 0
            background: Item {}

            onClicked: textInput.text = ""
        }
    }
}

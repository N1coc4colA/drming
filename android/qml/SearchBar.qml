import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar
    radius: 5
    color: palette.mid
    border.color: textInput.focus ? palette.highlight : palette.dark
    border.width: 1

    property bool darkMode: Application.styleHints.colorScheme === Qt.ColorScheme.Dark
    property alias searchText: textInput.text

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 5

        Image {
            fillMode: Image.PreserveAspectFit
            source: darkMode ? "qrc:/assets/dark/edit-find.svg" : "qrc:/assets/light/edit-find.svg"
        }

        TextField {
            id: textInput
            Layout.fillWidth: true
            placeholderText: qsTr("Search services...")
            background: Item {}
            font.pixelSize: 16
        }

        Button {
            icon.source: darkMode ? "qrc:/assets/dark/edit-clear.svg" : "qrc:/assets/light/edit-clear.svg"
            visible: textInput.text.length > 0
            onClicked: textInput.text = ""
            background: Item {}
        }
    }
}

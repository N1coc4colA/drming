import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar
    color: "#ffffff"
    radius: 8
    border.color: "#e0e0e0"
    border.width: 1

    property alias searchText: textInput.text

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Text {
            text: "🔍"
            font.pixelSize: 20
        }

        TextField {
            id: textInput
            Layout.fillWidth: true
            Layout.fillHeight: true
            placeholderText: qsTr("Search services...")
            background: Rectangle {
                color: "transparent"
            }
            font.pixelSize: 16
        }

        Button {
            text: "✕"
            visible: textInput.text.length > 0
            onClicked: textInput.text = ""
            background: Rectangle {
                color: "transparent"
            }
        }
    }
}

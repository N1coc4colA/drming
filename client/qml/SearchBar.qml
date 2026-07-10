import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: searchBar
    color: palette.mid

    border.color: textInput.focus ? palette.highlight : palette.dark
    border.width: 1
    radius: GlobalVars.standardRounding
    height: contentRow.height
    implicitHeight: contentRow.implicitHeight
    implicitWidth: contentRow.implicitWidth

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: GlobalVars.standardSpacing
        anchors.rightMargin: GlobalVars.standardSpacing
        spacing: GlobalVars.innerSpacing

        Image {
            fillMode: Image.PreserveAspectFit
            source: "qrc:/assets/edit-find.svg"
        }

        TextField {
            id: textInput
            font.pixelSize: Math.max(16, 16 * GlobalVars.scaling)
            placeholderText: qsTr("Search...")

            background: Item {}

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Button {
            icon.source: "qrc:/assets/edit-clear.svg"
            visible: textInput.text.length > 0

            background: Item {}

            onClicked: textInput.text = ""
        }
    }

    property alias searchText: textInput.text
}

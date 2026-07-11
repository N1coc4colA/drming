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
            cache: true
        }

        TextField {
            id: textInput
            font.pixelSize: GlobalVars.fontSizeMedium
            placeholderText: qsTr("Search...")

            background: Item {}

            Layout.fillWidth: true
            Layout.fillHeight: true

            onTextChanged: searchDebounceTimer.restart()
        }

        Button {
            icon.source: "qrc:/assets/edit-clear.svg"
            visible: textInput.text.length > 0

            background: Item {}

            onClicked: textInput.text = ""
        }
    }

    Timer {
        id: searchDebounceTimer
        interval: 150
        onTriggered: searchBar.searchTextChanged()
    }

    property alias searchText: textInput.text
}

import QtQuick.Controls

EasyPage {
    id: root

    property string searchText: ""

    signal back

    headerBar.centerContent: SearchBar {
        id: searchBar

        onSearchTextChanged: root.searchText = searchText
    }

    headerBar.leftContent: EasyButton {
        display: AbstractButton.IconOnly
        icon.source: "qrc:/assets/go-previous.svg"

        implicitWidth: headerBar.centerHeight
        implicitHeight: headerBar.centerHeight

        onClicked: root.back()
    }
}

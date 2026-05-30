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
        width: headerBar.centerHeight
        height: headerBar.centerHeight

        icon.source: "qrc:/assets/go-previous.svg"

        onClicked: root.back()
    }
}

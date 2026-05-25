import QtQuick.Controls

EasyPage {
    id: root

    property var selectedService: null
    property string searchText: ""

    signal back

    headerBar.centerContent: SearchBar {
        id: searchBar

        onSearchTextChanged: root.searchText = searchText
    }

    headerBar.leftContent: EasyButton {
        icon.source: "qrc:/assets/go-previous.svg"
        display: AbstractButton.IconOnly

        onClicked: root.back()

        height: headerBar.centerHeight
        width: headerBar.centerHeight
    }
}

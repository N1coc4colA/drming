import QtQuick

Item {
    id: root

    property bool overlayHeaderBar: false
    property bool headerBarVisible: true
    property Component overlay

    Loader {
        id: contentLoader

        anchors.fill: parent
        anchors.topMargin: root.overlayHeaderBar ? 0 : headerBarComponent.implicitHeight
    }

    Loader {
        id: overlayLoader

        anchors.fill: parent
        active: root.overlay !== null
        sourceComponent: root.overlay
        z: 1
    }

    HeaderBar {
        id: headerBarComponent

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        z: 2

        visible: root.overlayHeaderBar ? root.headerBarVisible : true
    }

    property alias content: contentLoader.sourceComponent
    property alias headerBar: headerBarComponent
}

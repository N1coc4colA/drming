import QtQuick
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        spacing: 0

        anchors.fill: parent

        HeaderBar {
            id: headerBarComponent

            Layout.fillWidth: true
        }

        Loader {
            id: contentLoader

            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    property alias content: contentLoader.sourceComponent
    property alias headerBar: headerBarComponent
}

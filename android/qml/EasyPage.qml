import QtQuick
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HeaderBar {
            id: headerBarComponent

            Layout.fillWidth: true
        }

        Loader {
            id: contentLoader

            Layout.fillHeight: true
            Layout.fillWidth: true
        }
    }

    property alias content: contentLoader.sourceComponent
    property alias headerBar: headerBarComponent
}

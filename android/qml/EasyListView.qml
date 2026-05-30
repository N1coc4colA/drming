import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property real topMargin: 5

    ListView {
        id: listView
        spacing: 8
        clip: true

        anchors.fill: parent

        ScrollBar.vertical: ScrollBar {
            id: scrollBar
            active: servicesModel.count > 0
            background: Rectangle {
                color: palette.dark
                opacity: scrollBar.contentItem.opacity
            }
        }

        header: Item {
            id: topSpacer
            height: topMargin * 2
            width: listView.width
        }

        footer: Item {
            height: listView.spacing
            width: listView.width
        }
    }

    Rectangle {
        height: topMargin + listView.spacing

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        gradient: Gradient {
            GradientStop { position: 0.0; color: palette.window }
            GradientStop { position: 0.5; color: palette.window }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Label {
        id: emptyLabel
        visible: model.count === 0

        anchors.centerIn: parent
        font.pixelSize: 16
    }

    property alias emptyText: emptyLabel.text
    property alias model: listView.model
    property alias delegate: listView.delegate
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    ListView {
        id: listView
        clip: true

        anchors.fill: parent
        spacing: GlobalVars.standardSpacing

        // Enable delegate reuse and caching for better scrolling performance
        cacheBuffer: 10
        reuseItems: true

        ScrollBar.vertical: ScrollBar {
            id: scrollBar
            active: model && model.count > 0

            background: Rectangle {
                color: palette.dark
                opacity: scrollBar.contentItem.opacity
            }
        }

        header: Item {
            id: topSpacer
            height: GlobalVars.doubleOutterSpacing / 2
            width: listView.width
        }

        footer: Item {
            height: listView.spacing
            width: listView.width
        }
    }

    // Fixed-height gradient overlay instead of full-height for better performance
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: GlobalVars.doubleOutterSpacing / 4 + listView.spacing

        gradient: Gradient {
            GradientStop { position: 0.0; color: palette.window }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Label {
        id: emptyLabel
        visible: !model || model.count === 0

        anchors.centerIn: parent

        font.pixelSize: GlobalVars.fontSizeLarge
    }

    property alias emptyText: emptyLabel.text
    property alias model: listView.model
    property alias delegate: listView.delegate
}

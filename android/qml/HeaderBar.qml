import QtQuick
import QtQuick.Layouts

Item {
    id: root

    implicitHeight: (root.twoRows
                     ? (twoRowLoader.item ? twoRowLoader.item.implicitHeight + 2*margin : 0)
                     : (oneRowLoader.item ? oneRowLoader.item.implicitHeight + 2*margin : 0))

    implicitWidth:  (root.twoRows
                     ? (twoRowLoader.item ? twoRowLoader.item.implicitWidth  : 0)
                     : (oneRowLoader.item ? oneRowLoader.item.implicitWidth  : 0))

    property Component leftContent
    property Component centerContent
    property Component rightContent

    readonly property bool hasLeft: !!leftContent
    readonly property bool hasCenter: !!centerContent
    readonly property bool hasRight: !!rightContent
    readonly property bool twoRows: hasLeft && hasCenter && hasRight && width < 300

    readonly property real spacing: 8
    readonly property real margin: 8

    readonly property real centerHeight: (centerContent && centerContent.item) ? centerContent.item.implicitHeight : 34

    Loader {
        id: oneRowLoader
        active: !root.twoRows
        width: parent.width
        sourceComponent: oneRowView
        // height is implicit from the loaded item
    }

    Loader {
        id: twoRowLoader
        active: root.twoRows
        width: parent.width
        sourceComponent: twoRowView
    }

    Component {
        id: oneRowView

        RowLayout {
            spacing: root.spacing

            anchors.margins: root.margin
            // Stretch horizontally inside the Loader, but keep vertical implicit
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            // Left item
            Loader {
                id: leftLoader
                active: visible
                sourceComponent: root.leftContent
                visible: root.hasLeft

                Layout.fillWidth: false
            }

            // Center item (or fallback spacer for left+right only)
            Loader {
                id: centerLoader
                active: visible
                sourceComponent: root.centerContent
                visible: root.hasCenter

                Layout.fillWidth: true
            }

            Item {
                id: middleSpacer
                visible: root.hasLeft && root.hasRight && !root.hasCenter

                Layout.fillWidth: true
            }

            // Right item
            Loader {
                id: rightLoader
                active: visible
                sourceComponent: root.rightContent
                visible: root.hasRight

                Layout.fillWidth: false
            }
        }
    }

    Component {
        id: twoRowView

        ColumnLayout {
            spacing: root.spacing

            anchors.margins: root.margin
            // Stretch horizontally inside the Loader, but keep vertical implicit
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            RowLayout {
                spacing: root.spacing

                Layout.fillWidth: true

                Loader {
                    id: leftLoader2
                    active: visible
                    sourceComponent: root.leftContent
                    visible: root.hasLeft

                    Layout.fillWidth: false
                }

                Item {
                    Layout.fillWidth: true
                }

                Loader {
                    id: rightLoader2
                    active: visible
                    sourceComponent: root.rightContent
                    visible: root.hasRight

                    Layout.fillWidth: false
                }
            }

            Loader {
                id: centerLoader2
                active: visible
                sourceComponent: root.centerContent
                visible: root.hasCenter

                Layout.fillWidth: true
            }
        }
    }
}

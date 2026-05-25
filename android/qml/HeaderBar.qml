import QtQuick
import QtQuick.Layouts

Item {
    id: root

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
        sourceComponent: oneRowView
        width: parent.width
        // height is implicit from the loaded item
    }

    Loader {
        id: twoRowLoader
        active: root.twoRows
        sourceComponent: twoRowView
        width: parent.width
    }

    implicitHeight: (root.twoRows
                     ? (twoRowLoader.item ? twoRowLoader.item.implicitHeight + 2*margin : 0)
                     : (oneRowLoader.item ? oneRowLoader.item.implicitHeight + 2*margin : 0))

    implicitWidth:  (root.twoRows
                     ? (twoRowLoader.item ? twoRowLoader.item.implicitWidth  : 0)
                     : (oneRowLoader.item ? oneRowLoader.item.implicitWidth  : 0))

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
                visible: root.hasLeft
                active: visible
                Layout.fillWidth: false
                sourceComponent: root.leftContent
            }

            // Center item (or fallback spacer for left+right only)
            Loader {
                id: centerLoader
                visible: root.hasCenter
                active: visible
                Layout.fillWidth: true
                sourceComponent: root.centerContent

                onItemChanged: if (item) {
                    // Stretch horizontally only
                    /*item.anchors.left = centerLoader
                    item.anchors.right = centerLoader*/
                }
            }

            Item {
                id: middleSpacer
                visible: root.hasLeft && root.hasRight && !root.hasCenter
                Layout.fillWidth: true
            }

            // Right item
            Loader {
                id: rightLoader
                visible: root.hasRight
                active: visible
                Layout.fillWidth: false
                sourceComponent: root.rightContent
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
                Layout.fillWidth: true
                spacing: root.spacing

                Loader {
                    id: leftLoader2
                    visible: root.hasLeft
                    active: visible
                    Layout.fillWidth: false
                    sourceComponent: root.leftContent
                }

                Item {
                    Layout.fillWidth: true
                }

                Loader {
                    id: rightLoader2
                    visible: root.hasRight
                    active: visible
                    Layout.fillWidth: false
                    sourceComponent: root.rightContent
                }
            }

            Loader {
                id: centerLoader2
                visible: root.hasCenter
                active: visible
                Layout.fillWidth: true
                sourceComponent: root.centerContent

                onItemChanged: if (item) {
                    /*item.anchors.left = centerLoader2
                    item.anchors.right = centerLoader2*/
                }
            }
        }
    }
}

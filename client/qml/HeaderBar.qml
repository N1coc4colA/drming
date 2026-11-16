import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: palette.window

    implicitHeight: loadedItem ? loadedItem.implicitHeight + 2*GlobalVars.standardSpacing : 0
    implicitWidth:  loadedItem ? loadedItem.implicitWidth  : 0

    property Component leftContent
    property Component centerContent
    property Component rightContent

    readonly property var loadedItem: root.twoRows ? twoRowLoader.item : oneRowLoader.item
    readonly property bool hasLeft: !!leftContent
    readonly property bool hasCenter: !!centerContent
    readonly property bool hasRight: !!rightContent
    readonly property bool twoRows: hasLeft && hasCenter && hasRight && width < GlobalVars.compactHeaderBreakpoint

    readonly property real centerHeight: (root.hasCenter && loadedItem && loadedItem.centerItem) ? loadedItem.centerItem.height : GlobalVars.buttonHeightDefault

    Loader {
        id: oneRowLoader
        active: !root.twoRows
        sourceComponent: oneRowView
        width: parent.width
    }

    Loader {
        id: twoRowLoader
        active: root.twoRows
        sourceComponent: twoRowView
        width: parent.width
    }

    Component {
        id: oneRowView

        RowLayout {
            anchors.margins: GlobalVars.standardSpacing
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: GlobalVars.standardSpacing

            Loader {
                id: leftLoader
                active: visible
                sourceComponent: root.leftContent
                visible: root.hasLeft
                Layout.alignment: Qt.AlignVCenter
            }

            Loader {
                id: centerLoader
                active: visible
                sourceComponent: root.centerContent
                visible: root.hasCenter
                Layout.fillWidth: true
            }

            Item {
                visible: root.hasLeft && root.hasRight && !root.hasCenter
                Layout.fillWidth: true
            }

            Loader {
                id: rightLoader
                active: visible
                sourceComponent: root.rightContent
                visible: root.hasRight
                Layout.alignment: Qt.AlignVCenter
            }

            readonly property alias centerItem: centerLoader.item
        }
    }

    Component {
        id: twoRowView

        ColumnLayout {
            anchors.margins: GlobalVars.standardSpacing
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: GlobalVars.standardSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: GlobalVars.standardSpacing

                Loader {
                    active: visible
                    sourceComponent: root.leftContent
                    visible: root.hasLeft
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                Loader {
                    active: visible
                    sourceComponent: root.rightContent
                    visible: root.hasRight
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            Loader {
                id: centerLoader
                active: visible
                sourceComponent: root.centerContent
                visible: root.hasCenter
                Layout.fillWidth: true
            }

            readonly property alias centerItem: centerLoader.item
        }
    }
}

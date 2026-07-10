pragma Singleton

import QtQml
import QtQuick

QtObject {
    id: root

    property var shaderBlurSource: null

    property var screen: null

    property real scaling: root.screen.pixelDensity/4

    property real standardRounding: 8
    property real innerRounding: 5

    property real outterSpacing: 10
    property real standardSpacing: 8
    property real innerSpacing: 5
}

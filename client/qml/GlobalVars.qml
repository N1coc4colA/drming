pragma Singleton

import QtQml
import QtQuick

QtObject {
    id: root

    property var shaderBlurSource: null

    property var screen: null

    property real scaling: root.screen ? root.screen.pixelDensity/4 : 1

    // Cached font sizes to avoid Math.max() calculations every frame
    property real fontSizeXSmall: Math.max(11, 11 * scaling)
    property real fontSizeSmall: Math.max(13, 13 * scaling)
    property real fontSizeMedium: Math.max(14, 14 * scaling)
    property real fontSizeBase: Math.max(16, 16 * scaling)
    property real fontSizeLarge: Math.max(18, 18 * scaling)
    property real fontSizeXLarge: Math.max(24, 24 * scaling)

    // Cached dimension sizes
    property real buttonHeightDefault: Math.max(34, 34 * scaling)
    property real rowHeightDefault: Math.max(40, 40 * scaling)
    property real spacingXSmall: Math.max(3, 3 * scaling)
    property real spacingSmall: Math.max(5, 5 * scaling)
    property real spacingBase: Math.max(10, 10 * scaling)

    property real standardRounding: 8
    property real innerRounding: 5

    property real outterSpacing: 10
    property real standardSpacing: 8
    property real innerSpacing: 5

    // Cache common calculations
    readonly property real doubleOutterSpacing: outterSpacing * 2
    readonly property real doubleStandardSpacing: standardSpacing * 2
    readonly property real halfStandardSpacing: standardSpacing * 0.5
}

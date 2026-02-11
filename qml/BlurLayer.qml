import QtQuick
import QtQuick.Effects

Item {
    id: root
    anchors.fill: parent

    required property Item sourceItem
    property rect sourceRect: Qt.rect(0, 0, width, height)

    ShaderEffectSource {
        id: capture
        anchors.fill: parent
        sourceItem: root.sourceItem
        sourceRect: root.sourceRect
        live: true
        hideSource: false
        smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        source: capture
        blurEnabled: true
        blurMax: 64
        blur: 0.82
        saturation: 1.02
        brightness: 0.03
        colorization: 0.04
        colorizationColor: "#ffffff"
    }
}

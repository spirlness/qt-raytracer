import QtQuick
import RayTracer 1.0

Item {
    id: root
    width: 1200
    height: 720
    property string appleFont: "SF Pro Text"

    property int cfgWidth: 400
    property int cfgHeight: 225
    property int cfgSamples: 24
    property int cfgDepth: 10
    property string aaPreset: "medium"
    property string computeBackendMode: "auto"
    property bool compactLayout: width < 980
    property bool effectsAvailable: false
    property var backendOptions: ["opengl", "vulkan", "d3d11", "metal", "software"]
    property var computeBackendOptions: ["auto", "opengl", "vulkan", "cuda", "cpu"]

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#f5f7fb" }
            GradientStop { position: 0.5; color: "#edf2f8" }
            GradientStop { position: 1.0; color: "#e7edf6" }
        }
    }

    Item {
        id: backdrop
        anchors.fill: parent

        Rectangle {
            width: root.width * 0.42
            height: width
            x: -width * 0.2
            y: -height * 0.3
            radius: width * 0.5
            color: "#8cb6ff"
            opacity: 0.28
        }

        Rectangle {
            width: root.width * 0.52
            height: width
            x: root.width - width * 0.75
            y: root.height - height * 0.55
            radius: width * 0.5
            color: "#b6e3ff"
            opacity: 0.36
        }
    }

    function clampInt(v, lo, hi, fallbackValue) {
        var n = parseInt(v)
        if (isNaN(n))
            return fallbackValue
        if (n < lo)
            return lo
        if (n > hi)
            return hi
        return n
    }

    function applySettings() {
        cfgWidth = clampInt(widthInput.text, 100, 3840, cfgWidth)
        cfgHeight = clampInt(heightInput.text, 100, 2160, cfgHeight)
        cfgSamples = clampInt(samplesInput.text, 1, 1000, cfgSamples)
        cfgDepth = clampInt(depthInput.text, 1, 100, cfgDepth)

        if (cfgSamples <= 12)
            aaPreset = "low"
        else if (cfgSamples <= 40)
            aaPreset = "medium"
        else
            aaPreset = "high"

        widthInput.text = cfgWidth.toString()
        heightInput.text = cfgHeight.toString()
        samplesInput.text = cfgSamples.toString()
        depthInput.text = cfgDepth.toString()

        rayItem.renderWidth = cfgWidth
        rayItem.renderHeight = cfgHeight
        rayItem.samples = cfgSamples
        rayItem.maxDepth = cfgDepth
        rayItem.computeBackend = computeBackendMode
    }

    function applyAAPreset(preset) {
        aaPreset = preset
        if (preset === "low")
            cfgSamples = 8
        else if (preset === "medium")
            cfgSamples = 24
        else
            cfgSamples = 64

        samplesInput.text = cfgSamples.toString()
        rayItem.samples = cfgSamples
    }

    Component.onCompleted: {
        var c = Qt.createComponent("qrc:/resources/qml/BlurLayer.qml")
        effectsAvailable = c.status === Component.Ready
    }

    Item {
        id: layoutRoot
        anchors.fill: parent
        anchors.margins: 16
        property int spacing: 14

        Rectangle {
            id: panelShadow
            x: panel.x + 2
            y: panel.y + 10
            width: panel.width
            height: panel.height
            radius: panel.radius
            color: "#111827"
            opacity: 0.12
        }

        Rectangle {
            id: panel
            x: 0
            y: 0
            width: root.compactLayout ? layoutRoot.width : Math.min(320, layoutRoot.width * 0.3)
            height: root.compactLayout ? Math.min(360, layoutRoot.height * 0.45) : layoutRoot.height
            radius: 16
            color: "#ffffff"
            opacity: root.effectsAvailable ? 0.56 : 0.92
            border.width: 1
            border.color: "#d6dce6"
            clip: true

            Loader {
                anchors.fill: parent
                active: root.effectsAvailable
                source: "qrc:/resources/qml/BlurLayer.qml"
                onLoaded: {
                    item.sourceItem = backdrop
                    item.sourceRect = Qt.binding(function() {
                        var p = panel.mapToItem(backdrop, 0, 0)
                        return Qt.rect(p.x, p.y, panel.width, panel.height)
                    })
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "#ffffff"
                opacity: 0.34
            }

            Flickable {
                anchors.fill: parent
                anchors.margins: 16
                contentWidth: width
                contentHeight: controlsColumn.implicitHeight
                clip: true

                Column {
                    id: controlsColumn
                    width: parent.width
                    spacing: 10

                    Text {
                        text: "Ray Tracer"
                        color: "#0f1728"
                        font.family: root.appleFont
                        font.pixelSize: 26
                        font.bold: true
                    }

                    Text {
                        text: "Settings"
                        color: "#6f7a8d"
                        font.family: root.appleFont
                        font.pixelSize: 14
                    }

                    Text {
                        text: "Graphics API"
                        color: "#667289"
                        font.family: root.appleFont
                        font.pixelSize: 13
                    }

                    Flow {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: root.backendOptions
                            delegate: Rectangle {
                                required property string modelData
                                property bool active: backendController.targetBackend === modelData

                                width: 76
                                height: 30
                                radius: 15
                                color: active ? "#e7f1ff" : "#f7f9fd"
                                border.width: 1
                                border.color: active ? "#7fb8ff" : "#d5dce8"

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    color: parent.active ? "#0a84ff" : "#5e6b82"
                                    font.family: root.appleFont
                                    font.pixelSize: 12
                                    font.weight: parent.active ? Font.DemiBold : Font.Medium
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: backendController.targetBackend = parent.modelData
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 34
                        radius: 10
                        color: "#f2f6fd"
                        border.width: 1
                        border.color: "#d5dce8"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            text: "Current: " + backendController.currentBackend
                            color: "#5e6b82"
                            font.family: root.appleFont
                            font.pixelSize: 12
                        }

                        Rectangle {
                            id: applyBackendButton
                            width: 86
                            height: 26
                            radius: 13
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: 4
                            color: backendController.targetBackend === backendController.currentBackend ? "#e5e9f1" : "#e7f1ff"
                            border.width: 1
                            border.color: backendController.targetBackend === backendController.currentBackend ? "#cfd6e3" : "#7fb8ff"

                            Text {
                                anchors.centerIn: parent
                                text: "Apply"
                                color: backendController.targetBackend === backendController.currentBackend ? "#8d99ad" : "#0a84ff"
                                font.family: root.appleFont
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: backendController.targetBackend !== backendController.currentBackend
                                onClicked: backendController.applyAndRestart()
                            }
                        }
                    }

                    Text {
                        text: "Path Tracing Kernel"
                        color: "#667289"
                        font.family: root.appleFont
                        font.pixelSize: 13
                    }

                    Flow {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: root.computeBackendOptions
                            delegate: Rectangle {
                                required property string modelData
                                property bool active: root.computeBackendMode === modelData

                                width: 76
                                height: 30
                                radius: 15
                                color: active ? "#e7f1ff" : "#f7f9fd"
                                border.width: 1
                                border.color: active ? "#7fb8ff" : "#d5dce8"

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    color: parent.active ? "#0a84ff" : "#5e6b82"
                                    font.family: root.appleFont
                                    font.pixelSize: 12
                                    font.weight: parent.active ? Font.DemiBold : Font.Medium
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.computeBackendMode = parent.modelData
                                        rayItem.computeBackend = root.computeBackendMode
                                    }
                                }
                            }
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: "#d3dae6"; opacity: 0.9 }

                    Text {
                        text: "AA Preset"
                        color: "#667289"
                        font.family: root.appleFont
                        font.pixelSize: 13
                    }

                    Row {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: [
                                { name: "Low", value: "low" },
                                { name: "Medium", value: "medium" },
                                { name: "High", value: "high" }
                            ]

                            delegate: Rectangle {
                                required property var modelData
                                property bool active: root.aaPreset === modelData.value

                                width: (parent.width - 16) / 3
                                height: 30
                                radius: 15
                                color: active ? "#e7f1ff" : "#f7f9fd"
                                border.width: 1
                                border.color: active ? "#7fb8ff" : "#d5dce8"

                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData.name
                                    color: parent.active ? "#0a84ff" : "#5e6b82"
                                    font.family: root.appleFont
                                    font.pixelSize: 12
                                    font.weight: parent.active ? Font.DemiBold : Font.Medium
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.applyAAPreset(parent.modelData.value)
                                }
                            }
                        }
                    }

                    Text { text: "Width"; color: "#667289"; font.family: root.appleFont; font.pixelSize: 13 }
                    Rectangle {
                        id: widthField
                        width: parent.width
                        height: 34
                        radius: 10
                        color: "#f8fafd"
                        border.width: 1
                        border.color: widthInput.activeFocus ? "#7fb8ff" : "#d5dce8"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.width: 2
                            border.color: "#63a9ff"
                            opacity: widthInput.activeFocus ? 0.5 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                        TextInput {
                            id: widthInput
                            anchors.fill: parent
                            anchors.margins: 8
                            text: root.cfgWidth.toString()
                            color: "#243248"
                            font.family: root.appleFont
                            enabled: !rayItem.rendering
                            selectByMouse: true
                            validator: IntValidator { bottom: 100; top: 3840 }
                        }
                    }

                    Text { text: "Height"; color: "#667289"; font.family: root.appleFont; font.pixelSize: 13 }
                    Rectangle {
                        id: heightField
                        width: parent.width
                        height: 34
                        radius: 10
                        color: "#f8fafd"
                        border.width: 1
                        border.color: heightInput.activeFocus ? "#7fb8ff" : "#d5dce8"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.width: 2
                            border.color: "#63a9ff"
                            opacity: heightInput.activeFocus ? 0.5 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                        TextInput {
                            id: heightInput
                            anchors.fill: parent
                            anchors.margins: 8
                            text: root.cfgHeight.toString()
                            color: "#243248"
                            font.family: root.appleFont
                            enabled: !rayItem.rendering
                            selectByMouse: true
                            validator: IntValidator { bottom: 100; top: 2160 }
                        }
                    }

                    Text { text: "Samples"; color: "#667289"; font.family: root.appleFont; font.pixelSize: 13 }
                    Rectangle {
                        id: samplesField
                        width: parent.width
                        height: 34
                        radius: 10
                        color: "#f8fafd"
                        border.width: 1
                        border.color: samplesInput.activeFocus ? "#7fb8ff" : "#d5dce8"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.width: 2
                            border.color: "#63a9ff"
                            opacity: samplesInput.activeFocus ? 0.5 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                        TextInput {
                            id: samplesInput
                            anchors.fill: parent
                            anchors.margins: 8
                            text: root.cfgSamples.toString()
                            color: "#243248"
                            font.family: root.appleFont
                            enabled: !rayItem.rendering
                            selectByMouse: true
                            validator: IntValidator { bottom: 1; top: 1000 }
                            onEditingFinished: root.applySettings()
                        }
                    }

                    Text { text: "Max Depth"; color: "#667289"; font.family: root.appleFont; font.pixelSize: 13 }
                    Rectangle {
                        id: depthField
                        width: parent.width
                        height: 34
                        radius: 10
                        color: "#f8fafd"
                        border.width: 1
                        border.color: depthInput.activeFocus ? "#7fb8ff" : "#d5dce8"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            color: "transparent"
                            border.width: 2
                            border.color: "#63a9ff"
                            opacity: depthInput.activeFocus ? 0.5 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                        TextInput {
                            id: depthInput
                            anchors.fill: parent
                            anchors.margins: 8
                            text: root.cfgDepth.toString()
                            color: "#243248"
                            font.family: root.appleFont
                            enabled: !rayItem.rendering
                            selectByMouse: true
                            validator: IntValidator { bottom: 1; top: 100 }
                        }
                    }

                    Rectangle {
                        id: actionButton
                        width: parent.width
                        height: 40
                        radius: 12
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: rayItem.rendering
                                       ? (buttonMouse.pressed ? "#e84f46" : buttonMouse.containsMouse ? "#ff746c" : "#ff6b63")
                                       : (buttonMouse.pressed ? "#0676f2" : buttonMouse.containsMouse ? "#53aaff" : "#3f9bff")
                            }
                            GradientStop {
                                position: 1.0
                                color: rayItem.rendering
                                       ? (buttonMouse.pressed ? "#dc372d" : buttonMouse.containsMouse ? "#ff5a50" : "#ff453a")
                                       : (buttonMouse.pressed ? "#0067d8" : buttonMouse.containsMouse ? "#1e90ff" : "#0a84ff")
                            }
                        }
                        border.width: 1
                        border.color: "#ffffff"
                        opacity: 0.95
                        scale: buttonMouse.pressed ? 0.985 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120 } }

                        Text {
                            anchors.centerIn: parent
                            color: "#ffffff"
                            font.family: root.appleFont
                            font.pixelSize: 14
                            font.bold: true
                            text: rayItem.rendering ? "Stop Render" : "Start Render"
                        }

                        MouseArea {
                            id: buttonMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (rayItem.rendering) {
                                    rayItem.stopRender()
                                } else {
                                    root.applySettings()
                                    rayItem.startRender()
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 12
                        radius: 6
                        color: "#e1e8f2"
                        border.width: 1
                        border.color: "#d2dae7"

                        Rectangle {
                            width: parent.width * (rayItem.progress / 100.0)
                            height: parent.height
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#59c8ff" }
                                GradientStop { position: 1.0; color: "#0a84ff" }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: "#2f3d53"
                        font.family: root.appleFont
                        font.pixelSize: 12
                        text: rayItem.statsText
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: "#7c879a"
                        font.family: root.appleFont
                        font.pixelSize: 11
                        text: root.effectsAvailable
                              ? "Glass UI (real blur enabled) · Adaptive layout"
                              : "Glass UI (fallback tint) · Adaptive layout"
                    }
                }
            }
        }

        Rectangle {
            id: viewportShadow
            x: viewportCard.x + 2
            y: viewportCard.y + 12
            width: viewportCard.width
            height: viewportCard.height
            radius: viewportCard.radius
            color: "#111827"
            opacity: 0.11
        }

        Rectangle {
            id: viewportCard
            x: root.compactLayout ? 0 : panel.width + layoutRoot.spacing
            y: root.compactLayout ? panel.height + layoutRoot.spacing : 0
            width: root.compactLayout
                   ? layoutRoot.width
                   : layoutRoot.width - panel.width - layoutRoot.spacing
            height: root.compactLayout
                    ? layoutRoot.height - panel.height - layoutRoot.spacing
                    : layoutRoot.height
            radius: 16
            color: "#ffffff"
            opacity: root.effectsAvailable ? 0.52 : 0.88
            border.width: 1
            border.color: "#d6dce6"
            clip: true

            Loader {
                anchors.fill: parent
                active: root.effectsAvailable
                source: "qrc:/resources/qml/BlurLayer.qml"
                onLoaded: {
                    item.sourceItem = backdrop
                    item.sourceRect = Qt.binding(function() {
                        var p = viewportCard.mapToItem(backdrop, 0, 0)
                        return Qt.rect(p.x, p.y, viewportCard.width, viewportCard.height)
                    })
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "#ffffff"
                opacity: 0.2
            }

            RayTracerFboItem {
                id: rayItem
                anchors.fill: parent
                anchors.margins: 12
                renderWidth: root.cfgWidth
                renderHeight: root.cfgHeight
                samples: root.cfgSamples
                maxDepth: root.cfgDepth
                computeBackend: root.computeBackendMode
            }
        }
    }
}

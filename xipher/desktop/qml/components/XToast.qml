import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

Item {
    id: root
    property int timeoutMs: 3500
    z: 1000

    function show(type, message) {
        if (!message || message.length === 0) {
            return;
        }
        toastModel.append({
            "kind": type,
            "message": message,
            "created": Date.now()
        });
    }

    ListModel {
        id: toastModel
    }

    Column {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingXxl
        spacing: Theme.spacingSm

        Repeater {
            model: toastModel
            delegate: Rectangle {
                id: toastItem
                width: Math.min(360, toastContent.implicitWidth + Theme.spacingXl * 2)
                height: toastContent.implicitHeight + Theme.spacingMd * 2
                radius: Theme.radiusLg
                
                // Background with glass effect
                color: {
                    switch (model.kind) {
                        case "error": return Qt.rgba(0.94, 0.27, 0.27, 0.15)
                        case "success": return Qt.rgba(0.06, 0.73, 0.51, 0.15)
                        case "warning": return Qt.rgba(0.96, 0.64, 0.35, 0.15)
                        default: return Theme.bgSecondary
                    }
                }
                
                border.color: {
                    switch (model.kind) {
                        case "error": return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.5)
                        case "success": return Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.5)
                        case "warning": return Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.5)
                        default: return Theme.glassBorder
                    }
                }
                border.width: 1

                // Shadow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: parent.radius + 2
                    color: "transparent"
                    z: -1
                    opacity: 0.3
                }

                Row {
                    id: toastContent
                    anchors.centerIn: parent
                    spacing: Theme.spacingSm

                    Text {
                        text: {
                            switch (model.kind) {
                                case "error": return "❌"
                                case "success": return "✅"
                                case "warning": return "⚠️"
                                default: return "ℹ️"
                            }
                        }
                        font.pixelSize: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: model.message
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        anchors.verticalCenter: parent.verticalCenter
                        wrapMode: Text.NoWrap
                    }
                }

                // Slide in animation
                opacity: 0
                y: 20

                Component.onCompleted: {
                    opacity = 1
                    y = 0
                }

                Behavior on opacity {
                    NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }

                Behavior on y {
                    NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
                }

                // Progress bar for timeout
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 4
                    height: 2
                    radius: 1
                    color: Qt.rgba(1, 1, 1, 0.1)

                    Rectangle {
                        id: progressBar
                        anchors.left: parent.left
                        height: parent.height
                        radius: parent.radius
                        color: {
                            switch (model.kind) {
                                case "error": return Theme.error
                                case "success": return Theme.success
                                case "warning": return Theme.warning
                                default: return Theme.accentSoft
                            }
                        }
                        width: parent.width

                        NumberAnimation on width {
                            from: progressBar.parent.width
                            to: 0
                            duration: root.timeoutMs
                            easing.type: Easing.Linear
                        }
                    }
                }

                Timer {
                    interval: root.timeoutMs
                    running: true
                    repeat: false
                    onTriggered: {
                        toastItem.opacity = 0
                        toastItem.y = 20
                        removeTimer.start()
                    }
                }

                Timer {
                    id: removeTimer
                    interval: Theme.animNormal
                    onTriggered: toastModel.remove(index)
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        toastItem.opacity = 0
                        removeTimer.start()
                    }
                }
            }
        }
    }
}

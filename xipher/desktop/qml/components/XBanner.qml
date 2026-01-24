import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

Rectangle {
    id: root
    property string text: ""
    property string variant: "info" // "info", "error", "warning", "success"

    height: visible ? 44 : 0
    width: parent ? parent.width : 300
    visible: text.length > 0
    
    // Background color based on variant
    color: {
        switch (variant) {
            case "error": return Qt.rgba(0.94, 0.27, 0.27, 0.12)
            case "warning": return Qt.rgba(0.96, 0.64, 0.35, 0.12)
            case "success": return Qt.rgba(0.06, 0.73, 0.51, 0.12)
            default: return Qt.rgba(Theme.purplePrimary.r, Theme.purplePrimary.g, Theme.purplePrimary.b, 0.12)
        }
    }
    
    border.color: {
        switch (variant) {
            case "error": return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.3)
            case "warning": return Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.3)
            case "success": return Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.3)
            default: return Qt.rgba(Theme.purplePrimary.r, Theme.purplePrimary.g, Theme.purplePrimary.b, 0.3)
        }
    }
    border.width: 1

    Behavior on height {
        NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }

    Row {
        anchors.centerIn: parent
        spacing: Theme.spacingSm

        Text {
            text: {
                switch (root.variant) {
                    case "error": return "⚠️"
                    case "warning": return "⚡"
                    case "success": return "✅"
                    default: return "ℹ️"
                }
            }
            font.pixelSize: 14
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            color: {
                switch (root.variant) {
                    case "error": return Theme.error
                    case "warning": return Theme.warning
                    case "success": return Theme.success
                    default: return Theme.textPrimary
                }
            }
            font.family: Theme.fontFamily
            font.pixelSize: 13
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // Animated pulse for error state
    Rectangle {
        anchors.fill: parent
        color: Theme.error
        opacity: 0
        visible: root.variant === "error"

        SequentialAnimation on opacity {
            running: root.variant === "error" && root.visible
            loops: 2
            NumberAnimation { to: 0.1; duration: 300 }
            NumberAnimation { to: 0; duration: 300 }
        }
    }
}

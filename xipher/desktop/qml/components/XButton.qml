import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style button
Button {
    id: root
    property string variant: "primary" // "primary", "secondary", "ghost", "danger"

    implicitHeight: 42
    implicitWidth: Math.max(120, contentItem.implicitWidth + Theme.spacingXl * 2)

    background: Rectangle {
        id: bg
        radius: Theme.radiusSm
        
        color: {
            if (!root.enabled) return Theme.bgTertiary
            
            switch (root.variant) {
                case "primary":
                    return root.pressed ? Theme.primaryDark : (root.hovered ? Theme.primaryHover : Theme.primary)
                case "secondary":
                    return root.pressed ? Theme.bgTertiary : (root.hovered ? Theme.bgTertiary : Theme.bgSecondary)
                case "ghost":
                    return root.hovered ? Theme.bgTertiary : "transparent"
                case "danger":
                    return root.pressed ? "#C62828" : (root.hovered ? "#E53935" : "transparent")
                default:
                    return Theme.bgTertiary
            }
        }
        
        border.color: {
            if (root.variant === "secondary") return Theme.borderColor
            if (root.variant === "danger" && !root.hovered) return Theme.error
            return "transparent"
        }
        border.width: root.variant === "secondary" || (root.variant === "danger" && !root.hovered) ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    contentItem: Text {
        text: root.text
        color: {
            if (!root.enabled) return Theme.textMuted
            switch (root.variant) {
                case "primary":
                    return "#FFFFFF"
                case "danger":
                    return root.hovered ? "#FFFFFF" : Theme.error
                default:
                    return Theme.textPrimary
            }
        }
        font.family: Theme.fontFamily
        font.pixelSize: 14
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

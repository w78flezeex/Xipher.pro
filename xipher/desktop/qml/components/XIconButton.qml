import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style icon button
Button {
    id: root
    property string iconText: ""
    property int size: Theme.minHit
    property string variant: "default" // "default", "ghost", "primary"

    implicitHeight: size
    implicitWidth: size

    background: Rectangle {
        radius: size / 2  // Round buttons
        
        color: {
            if (!root.enabled) return "transparent"
            
            switch (root.variant) {
                case "primary":
                    return root.pressed 
                        ? Theme.primaryDark 
                        : (root.hovered ? Theme.primaryHover : Theme.primary)
                case "ghost":
                    return root.hovered ? Theme.bgTertiary : "transparent"
                default:
                    return root.hovered ? Theme.bgTertiary : "transparent"
            }
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    contentItem: Text {
        text: root.iconText
        color: {
            if (!root.enabled) return Theme.textMuted
            if (root.variant === "primary") return "#FFFFFF"
            return root.hovered ? Theme.textPrimary : Theme.textSecondary
        }
        font.pixelSize: size * 0.45
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

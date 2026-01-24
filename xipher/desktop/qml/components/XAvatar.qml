import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style avatar component
Item {
    id: root
    property string text: ""
    property string name: text
    property string imageUrl: ""
    property int size: Theme.avatarMd
    property bool online: false
    property bool showOnlineIndicator: true
    property int colorIndex: 0  // For consistent avatar colors

    width: size
    height: size

    // Calculate color index from name for consistent coloring
    function getColorIndex() {
        var str = root.text || root.name || ""
        if (str.length === 0) return 0
        var hash = 0
        for (var i = 0; i < str.length; i++) {
            hash = str.charCodeAt(i) + ((hash << 5) - hash)
        }
        return Math.abs(hash) % 8
    }

    // Avatar circle - flat color, Telegram style
    Rectangle {
        id: avatarBg
        anchors.fill: parent
        radius: size / 2
        clip: true
        color: Theme.avatarColor(root.colorIndex > 0 ? root.colorIndex : getColorIndex())

        // Initials text
        Text {
            anchors.centerIn: parent
            text: {
                var displayText = root.text || root.name || ""
                if (displayText.length === 0) return "?"
                var parts = displayText.trim().split(/\s+/)
                if (parts.length >= 2) {
                    return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
                }
                return displayText.charAt(0).toUpperCase()
            }
            color: "#FFFFFF"
            font.family: Theme.fontFamily
            font.pixelSize: size * 0.40
            font.weight: Font.Medium
            visible: !avatarImage.visible
        }

        // Avatar image
        Image {
            id: avatarImage
            anchors.fill: parent
            source: root.imageUrl
            fillMode: Image.PreserveAspectCrop
            visible: status === Image.Ready && root.imageUrl.length > 0
            asynchronous: true
            cache: true
        }
    }

    // Online indicator - Telegram style (green dot)
    Rectangle {
        id: onlineIndicator
        width: Math.max(size * 0.26, 12)
        height: width
        radius: width / 2
        color: Theme.online
        border.color: Theme.bgPrimary
        border.width: Math.max(2, size * 0.05)
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 0
        anchors.bottomMargin: 0
        visible: root.showOnlineIndicator && root.online
        z: 10
    }
}

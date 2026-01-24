import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style chat list item
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string time: ""
    property string avatarText: ""
    property string avatarUrl: ""
    property int unread: 0
    property bool online: false
    property bool selected: false
    property bool pinned: false
    property bool muted: false

    signal clicked()

    height: Theme.chatItemHeight
    width: parent ? parent.width : 320

    // Selection/hover background - Telegram style (flat, no rounded corners on sides)
    Rectangle {
        anchors.fill: parent
        color: root.selected 
            ? Theme.bgSidebarActive
            : (mouseArea.containsMouse ? Theme.bgSidebarHover : "transparent")
        
        Behavior on color {
            ColorAnimation { duration: Theme.animFast }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd

        // Avatar
        XAvatar {
            anchors.verticalCenter: parent.verticalCenter
            size: Theme.avatarLg
            text: root.avatarText
            imageUrl: root.avatarUrl
            online: root.online
            showOnlineIndicator: true
        }

        // Chat info
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            width: parent.width - Theme.avatarLg - 70 - Theme.spacingMd * 2

            // Title row with time
            Item {
                width: parent.width
                height: titleText.height

                Text {
                    id: titleText
                    text: root.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    width: parent.width - timeText.width - Theme.spacingXs
                }

                Text {
                    id: timeText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.time
                    color: root.unread > 0 ? Theme.primary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }
            }

            // Subtitle row with indicators
            Item {
                width: parent.width
                height: subtitleText.height

                Text {
                    id: subtitleText
                    text: root.subtitle
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    width: parent.width - indicatorsRow.width - Theme.spacingXs
                }

                Row {
                    id: indicatorsRow
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    // Pinned icon
                    Text {
                        text: "📌"
                        font.pixelSize: 12
                        visible: root.pinned && root.unread === 0
                        opacity: 0.6
                    }

                    // Muted icon
                    Text {
                        text: "🔇"
                        font.pixelSize: 11
                        visible: root.muted
                        opacity: 0.5
                    }

                    // Unread badge - Telegram style (rounded pill)
                    Rectangle {
                        visible: root.unread > 0
                        radius: height / 2
                        height: 20
                        width: Math.max(20, unreadLabel.width + 10)
                        color: root.muted ? Theme.textMuted : Theme.primary

                        Text {
                            id: unreadLabel
                            anchors.centerIn: parent
                            text: root.unread > 99 ? "99+" : root.unread
                            color: "#FFFFFF"
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }
    }

    // Bottom separator line
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: Theme.avatarLg + Theme.spacingMd * 2
        anchors.right: parent.right
        height: 1
        color: Theme.borderSubtle
        visible: !root.selected
    }
}

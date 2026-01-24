import QtQuick 2.15
import QtQuick.Controls 2.15
import Xipher.Desktop

// Telegram-style message bubble
Item {
    id: root
    property string content: ""
    property string time: ""
    property string status: ""
    property bool sent: false
    property string messageType: "text"
    property string messageId: ""
    property string tempId: ""
    property string fileName: ""
    property string filePath: ""
    property string localFilePath: ""
    property string transferState: ""
    property int transferProgress: 0
    property string replyToText: ""
    property string replyToAuthor: ""
    property string senderName: ""
    property string senderAvatar: ""
    property bool showAvatar: true
    property bool isFirst: false
    property bool isLast: true

    signal copyRequested(string text)
    signal deleteRequested(string messageId)
    signal retryRequested(string messageId)
    signal replyRequested()

    width: parent ? parent.width : 400
    height: messageRow.height + (isLast ? Theme.spacingSm : 2)

    Row {
        id: messageRow
        anchors.right: root.sent ? parent.right : undefined
        anchors.left: root.sent ? undefined : parent.left
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingSm
        layoutDirection: root.sent ? Qt.RightToLeft : Qt.LeftToRight

        // Avatar (only for received messages)
        Item {
            width: Theme.avatarSm
            height: Theme.avatarSm
            visible: !root.sent && root.showAvatar
            anchors.bottom: parent.bottom

            XAvatar {
                anchors.fill: parent
                size: Theme.avatarSm
                text: root.senderName
                imageUrl: root.senderAvatar
                showOnlineIndicator: false
                visible: root.isLast
            }
        }

        // Spacer for alignment when avatar hidden
        Item {
            width: Theme.avatarSm
            height: 1
            visible: !root.sent && !root.showAvatar
        }

        // Message bubble - Telegram style (flat, no gradient)
        Rectangle {
            id: bubble
            radius: Theme.radiusBubble
            width: Math.min(root.width * 0.70, Math.max(80, contentColumn.implicitWidth + 24))
            height: contentColumn.implicitHeight + 16

            // Flat color - Telegram style
            color: root.sent ? Theme.msgBubbleOut : Theme.msgBubbleIn
            border.color: root.sent ? "transparent" : Theme.msgBubbleInBorder
            border.width: root.sent ? 0 : 1

            Column {
                id: contentColumn
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 4

                // Sender name for group chats
                Text {
                    visible: !root.sent && root.senderName.length > 0 && root.isFirst
                    text: root.senderName
                    color: Theme.primary
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                // Reply preview
                Rectangle {
                    visible: root.replyToText.length > 0
                    width: Math.min(contentColumn.width - 4, 220)
                    height: replyColumn.height + 8
                    radius: 4
                    color: root.sent ? Qt.rgba(0, 0, 0, 0.1) : Theme.bgTertiary

                    Row {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 6

                        Rectangle {
                            width: 2
                            height: parent.height
                            radius: 1
                            color: Theme.primary
                        }

                        Column {
                            id: replyColumn
                            spacing: 2
                            width: parent.width - 10

                            Text {
                                text: root.replyToAuthor
                                color: Theme.primary
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }

                            Text {
                                text: root.replyToText
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }
                    }
                }

                // Text content
                Text {
                    id: textItem
                    text: root.content
                    color: Theme.textPrimary
                    wrapMode: Text.Wrap
                    width: Math.min(implicitWidth, root.width * 0.62)
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    lineHeight: 1.35
                    visible: root.content.length > 0 && root.messageType === "text"
                }

                // Image attachment
                Rectangle {
                    visible: root.messageType === "image" && root.localFilePath.length > 0
                    width: Math.min(contentColumn.width, 300)
                    height: width * 0.75
                    radius: 8
                    color: Theme.bgTertiary
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: root.localFilePath
                        fillMode: Image.PreserveAspectCrop

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Progress overlay
                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(0, 0, 0, 0.5)
                        visible: root.transferState === "uploading"

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            BusyIndicator {
                                anchors.horizontalCenter: parent.horizontalCenter
                                running: true
                                width: 28
                                height: 28
                            }

                            Text {
                                text: root.transferProgress + "%"
                                color: "#FFFFFF"
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                            }
                        }
                    }
                }

                // File attachment - Telegram style
                Rectangle {
                    visible: root.fileName.length > 0 && root.messageType !== "image"
                    radius: 6
                    color: root.sent ? Qt.rgba(0, 0, 0, 0.1) : Theme.bgTertiary
                    height: 44
                    width: Math.min(contentColumn.width, 220)

                    Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        Rectangle {
                            width: 28
                            height: 28
                            radius: 6
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: root.messageType == "voice" ? "🎤" : "📄"
                                font.pixelSize: 14
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            width: parent.width - 44

                            Text {
                                text: root.fileName
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                elide: Text.ElideMiddle
                                width: parent.width
                            }

                            Text {
                                text: root.transferState === "uploading" 
                                    ? root.transferProgress + "%" 
                                    : ""
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 11
                                visible: text.length > 0
                            }
                        }
                    }
                }

                // Failed state
                Row {
                    visible: root.transferState === "failed" || root.status === "failed"
                    spacing: 6

                    Text {
                        text: "⚠️"
                        font.pixelSize: 11
                    }

                    Text {
                        text: SettingsStore.language === "ru" ? "Ошибка отправки" : "Failed to send"
                        color: Theme.error
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                    }

                    Text {
                        text: SettingsStore.language === "ru" ? "Повторить" : "Retry"
                        color: Theme.primary
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.retryRequested(root.messageId.length ? root.messageId : root.tempId)
                        }
                    }
                }

                // Time and status row - Telegram style
                Row {
                    spacing: 4
                    anchors.right: parent.right

                    Text {
                        text: root.time
                        color: root.sent ? Theme.msgTimeSent : Theme.msgTimeReceived
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        opacity: 0.7
                    }

                    // Status checks - Telegram style
                    Text {
                        visible: root.sent
                        text: {
                            if (root.status === "read") return "✓✓"
                            if (root.status === "delivered") return "✓✓"
                            if (root.status === "sent") return "✓"
                            if (root.status === "sending") return "🕐"
                            return ""
                        }
                        color: root.status === "read" ? Theme.msgCheck : Theme.msgTimeSent
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        font.letterSpacing: -2
                    }
                }
            }
        }
    }

    // Context menu - Telegram style
    Menu {
        id: contextMenu
        
        background: Rectangle {
            implicitWidth: 180
            color: Theme.bgSecondary
            border.color: Theme.borderColor
            border.width: 1
            radius: 8
        }
        
        MenuItem {
            text: "↩️ " + (SettingsStore.language === "ru" ? "Ответить" : "Reply")
            onTriggered: root.replyRequested()
            
            background: Rectangle {
                color: parent.highlighted ? Theme.bgTertiary : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textPrimary
                font.pixelSize: 14
            }
        }
        MenuItem {
            text: "📋 " + (SettingsStore.language === "ru" ? "Копировать" : "Copy")
            enabled: root.content.length > 0
            onTriggered: root.copyRequested(root.content)
            
            background: Rectangle {
                color: parent.highlighted ? Theme.bgTertiary : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? Theme.textPrimary : Theme.textMuted
                font.pixelSize: 14
            }
        }
        MenuItem {
            text: "↪️ " + (SettingsStore.language === "ru" ? "Переслать" : "Forward")
            enabled: root.messageId.length > 0
            onTriggered: {
                console.log("Forward message:", root.messageId)
            }
            
            background: Rectangle {
                color: parent.highlighted ? Theme.bgTertiary : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? Theme.textPrimary : Theme.textMuted
                font.pixelSize: 14
            }
        }
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.borderColor
            }
        }
        MenuItem {
            text: "🗑️ " + (SettingsStore.language === "ru" ? "Удалить" : "Delete")
            onTriggered: root.deleteRequested(root.messageId.length ? root.messageId : root.tempId)
            
            background: Rectangle {
                color: parent.highlighted ? Theme.bgTertiary : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: Theme.error
                font.pixelSize: 14
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        hoverEnabled: true
        
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
    }
}

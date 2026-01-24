import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop

// Telegram-style chat header
Rectangle {
    id: root
    
    property string title: ""
    property string subtitle: ""
    property string avatarText: ""
    property string avatarUrl: ""
    property bool online: false
    property bool isTyping: false
    property string typingText: ""

    signal callRequested()
    signal videoCallRequested()
    signal searchRequested()
    signal menuRequested()
    signal infoRequested()

    height: Theme.headerHeight
    color: Theme.bgHeader

    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd

        // Avatar and info
        MouseArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.infoRequested()

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingMd

                XAvatar {
                    size: Theme.avatarMd
                    text: root.avatarText
                    imageUrl: root.avatarUrl
                    online: root.online
                    showOnlineIndicator: false
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: root.title
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Row {
                        spacing: 4
                        visible: root.isTyping || root.subtitle.length > 0

                        Text {
                            text: root.isTyping ? root.typingText : root.subtitle
                            color: root.isTyping ? Theme.primary : (root.online ? Theme.online : Theme.textSecondary)
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                        }

                        // Typing dots - Telegram style
                        Row {
                            spacing: 2
                            visible: root.isTyping
                            anchors.verticalCenter: parent.verticalCenter

                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 5
                                    height: 5
                                    radius: 2.5
                                    color: Theme.primary

                                    SequentialAnimation on opacity {
                                        running: root.isTyping
                                        loops: Animation.Infinite
                                        PauseAnimation { duration: index * 150 }
                                        NumberAnimation { to: 0.3; duration: 200 }
                                        NumberAnimation { to: 1.0; duration: 200 }
                                        PauseAnimation { duration: (2 - index) * 150 }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Action buttons - Telegram style
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingXs

            XIconButton {
                iconText: "📞"
                size: 40
                variant: "ghost"
                onClicked: root.callRequested()
            }

            XIconButton {
                iconText: "🔍"
                size: 40
                variant: "ghost"
                onClicked: root.searchRequested()
            }

            XIconButton {
                iconText: "⋮"
                size: 40
                variant: "ghost"
                onClicked: root.menuRequested()
            }
        }
    }
}

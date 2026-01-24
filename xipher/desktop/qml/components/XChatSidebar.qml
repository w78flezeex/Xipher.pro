import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../i18n/Translations.js" as I18n

// Telegram-style sidebar
Rectangle {
    id: root

    property string currentUserId: ""
    property string currentUserName: ""
    property string currentUserAvatar: ""
    property alias searchQuery: searchField.text
    property alias model: chatList.model
    property string selectedChatId: ""
    property bool loading: false
    property int activeTab: 0

    signal chatSelected(string chatId)
    signal newChatRequested()
    signal settingsRequested()
    signal profileRequested()
    signal searchChanged(string query)

    width: Theme.sidebarWidth
    color: Theme.bgSidebar

    // Right border
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.borderColor
        z: 10
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header - Telegram style
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.bgHeader

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd

                // Hamburger menu button
                XIconButton {
                    iconText: "☰"
                    size: 40
                    variant: "ghost"
                    onClicked: root.profileRequested()
                }

                // Search field in header - Telegram style
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 18
                    color: Theme.bgTertiary

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingSm

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "🔍"
                            font.pixelSize: 14
                            opacity: 0.5
                        }

                        TextInput {
                            id: searchField
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 30
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            clip: true
                            selectByMouse: true
                            onTextChanged: root.searchChanged(text)

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: I18n.t("search.placeholder", SettingsStore.language)
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 14
                                visible: parent.text.length === 0
                            }
                        }
                    }
                }
            }
        }

        // Tabs - Telegram style (horizontal scroll)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.bgHeader

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderColor
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                spacing: 0

                Repeater {
                    model: [
                        { text: I18n.t("tabs.all", SettingsStore.language), icon: "💬" },
                        { text: I18n.t("tabs.personal", SettingsStore.language), icon: "👤" },
                        { text: I18n.t("tabs.groups", SettingsStore.language), icon: "👥" }
                    ]

                    delegate: Item {
                        width: tabText.implicitWidth + Theme.spacingXl
                        height: parent.height

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeTab = index

                            Rectangle {
                                anchors.fill: parent
                                color: parent.containsMouse && root.activeTab !== index 
                                    ? Theme.bgSidebarHover : "transparent"
                            }
                        }

                        Text {
                            id: tabText
                            anchors.centerIn: parent
                            text: modelData.text
                            color: root.activeTab === index ? Theme.primary : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            font.weight: root.activeTab === index ? Font.Medium : Font.Normal
                        }

                        // Active tab indicator
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: tabText.width
                            height: 3
                            radius: 1.5
                            color: Theme.primary
                            visible: root.activeTab === index
                        }
                    }
                }

                // Spacer
                Item { Layout.fillWidth: true; width: 10 }

                // New chat button
                XIconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacingMd
                    iconText: "✏️"
                    size: 36
                    variant: "ghost"
                    onClicked: root.newChatRequested()
                    ToolTip.text: I18n.t("chat.new", SettingsStore.language)
                    ToolTip.visible: hovered
                }
            }
        }

        // Chat list - Telegram style
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: chatList
                anchors.fill: parent
                clip: true
                spacing: 0

                delegate: XChatItem {
                    width: chatList.width
                    title: model.displayName && model.displayName.length > 0 ? model.displayName : model.name
                    subtitle: model.lastMessage
                    time: model.time
                    avatarText: model.avatar
                    avatarUrl: model.avatarUrl || ""
                    unread: model.unread
                    online: model.online
                    selected: model.id === root.selectedChatId
                    pinned: model.pinned
                    muted: model.muted
                    onClicked: root.chatSelected(model.id)
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 6
                }

                // Empty state
                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingLg
                    visible: !root.loading && chatList.count === 0

                    Text {
                        text: "💬"
                        font.pixelSize: 64
                        anchors.horizontalCenter: parent.horizontalCenter
                        opacity: 0.3
                    }

                    Text {
                        text: I18n.t("chats.empty", SettingsStore.language)
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 15
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            // Loading skeleton - Telegram style
            Column {
                anchors.fill: parent
                spacing: 0
                visible: root.loading

                Repeater {
                    model: 8
                    delegate: Rectangle {
                        height: Theme.chatItemHeight
                        width: parent.width
                        color: Theme.bgSidebar

                        Row {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMd
                            spacing: Theme.spacingMd

                            Rectangle {
                                width: Theme.avatarLg
                                height: Theme.avatarLg
                                radius: Theme.avatarLg / 2
                                color: Theme.bgTertiary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8

                                Rectangle {
                                    width: 140
                                    height: 14
                                    radius: 4
                                    color: Theme.bgTertiary
                                }

                                Rectangle {
                                    width: 200
                                    height: 12
                                    radius: 4
                                    color: Theme.bgTertiary
                                    opacity: 0.6
                                }
                            }
                        }

                        SequentialAnimation on opacity {
                            running: root.loading
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.5; duration: 600 }
                            NumberAnimation { to: 1.0; duration: 600 }
                        }
                    }
                }
            }
        }
    }
}

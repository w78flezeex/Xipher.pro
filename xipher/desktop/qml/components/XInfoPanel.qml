import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../i18n/Translations.js" as I18n

// Telegram-style right info panel
Rectangle {
    id: root

    property string chatTitle: ""
    property string chatSubtitle: ""
    property string chatAvatarText: ""
    property string chatAvatarUrl: ""
    property bool chatOnline: false
    property var members: []
    property var sharedMedia: []

    signal settingsRequested()
    signal muteToggled()
    signal pinToggled()
    signal clearHistoryRequested()
    signal deleteChatRequested()

    width: Theme.infoPanelWidth
    color: Theme.bgSidebar

    // Left border
    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.borderColor
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header with close button
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.bgHeader

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

                Text {
                    text: I18n.t("info.title", SettingsStore.language)
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                XIconButton {
                    iconText: "✕"
                    size: 36
                    variant: "ghost"
                }
            }
        }

        // Scrollable content
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 0

                // Profile section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: profileColumn.implicitHeight + Theme.spacingXl * 2
                    color: "transparent"

                    Column {
                        id: profileColumn
                        anchors.centerIn: parent
                        spacing: Theme.spacingSm

                        XAvatar {
                            anchors.horizontalCenter: parent.horizontalCenter
                            size: Theme.avatarXl
                            text: root.chatAvatarText
                            imageUrl: root.chatAvatarUrl
                            online: root.chatOnline
                            showOnlineIndicator: false
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.chatTitle
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 16
                            font.weight: Font.Medium
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.chatOnline ? I18n.t("status.online", SettingsStore.language) : I18n.t("status.offline", SettingsStore.language)
                            color: root.chatOnline ? Theme.online : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            visible: root.chatTitle.length > 0
                        }
                    }
                }

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 8
                    color: Theme.bgTertiary
                }

                // Actions list - Telegram style
                Column {
                    Layout.fillWidth: true
                    visible: root.chatTitle.length > 0

                    XInfoActionButton {
                        width: parent.width
                        icon: "🔔"
                        text: I18n.t("info.notifications", SettingsStore.language)
                        onClicked: root.muteToggled()
                    }

                    XInfoActionButton {
                        width: parent.width
                        icon: "🔍"
                        text: I18n.t("info.search", SettingsStore.language)
                    }
                }

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 8
                    color: Theme.bgTertiary
                    visible: root.chatTitle.length > 0
                }

                // Shared media section
                Column {
                    Layout.fillWidth: true
                    visible: root.chatTitle.length > 0

                    XInfoActionButton {
                        width: parent.width
                        icon: "📁"
                        text: I18n.t("info.media", SettingsStore.language)
                        showArrow: true
                    }

                    XInfoActionButton {
                        width: parent.width
                        icon: "🔗"
                        text: I18n.t("info.links", SettingsStore.language)
                        showArrow: true
                    }

                    XInfoActionButton {
                        width: parent.width
                        icon: "📄"
                        text: I18n.t("info.files", SettingsStore.language)
                        showArrow: true
                    }
                }

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 8
                    color: Theme.bgTertiary
                    visible: root.chatTitle.length > 0
                }

                // Danger zone
                Column {
                    Layout.fillWidth: true
                    visible: root.chatTitle.length > 0

                    XInfoActionButton {
                        width: parent.width
                        icon: "🗑️"
                        text: I18n.t("info.clearHistory", SettingsStore.language)
                        danger: true
                        onClicked: root.clearHistoryRequested()
                    }

                    XInfoActionButton {
                        width: parent.width
                        icon: "❌"
                        text: I18n.t("info.deleteChat", SettingsStore.language)
                        danger: true
                        onClicked: root.deleteChatRequested()
                    }
                }
            }
        }
    }
}

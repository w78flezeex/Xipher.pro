import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../components"
import "../i18n/Translations.js" as I18n

Popup {
    id: root
    modal: true
    focus: true
    width: Math.min(parent.width * 0.9, 480)
    height: Math.min(parent.height * 0.85, 550)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    property int mode: 0 // 0 = find user, 1 = create group
    property var foundUser: null
    property var selectedMembers: []
    property bool searching: false

    signal chatStarted(string chatId)

    onOpened: {
        searchField.text = ""
        foundUser = null
        selectedMembers = []
        mode = 0
        searching = false
    }

    Connections {
        target: ShellViewModel
        function onUserFound(success, userId, username, avatarUrl, isPremium, message) {
            root.searching = false
            if (success) {
                root.foundUser = {
                    userId: userId,
                    username: username,
                    avatarUrl: avatarUrl || "",
                    isPremium: isPremium
                }
            } else {
                root.foundUser = null
            }
        }
        
        function onGroupCreated(success, groupId, message) {
            if (success && groupId) {
                root.chatStarted(groupId)
                root.close()
            }
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.bgSidebar
        border.color: Theme.borderColor
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.bgSidebar
            
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

                Text {
                    text: root.mode === 0 
                        ? (I18n.t("dialog.newChat.title", SettingsStore.language) || "Новый чат")
                        : (I18n.t("dialog.createGroup.title", SettingsStore.language) || "Создать группу")
                    font.family: Theme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                XIconButton {
                    size: 34
                    iconText: "✕"
                    variant: "ghost"
                    onClicked: root.close()
                }
            }
        }

        // Tab buttons
        Row {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLg
            Layout.rightMargin: Theme.spacingLg
            Layout.topMargin: Theme.spacingMd
            spacing: Theme.spacingSm

            XButton {
                text: I18n.t("dialog.newChat.findUser", SettingsStore.language) || "Найти пользователя"
                variant: root.mode === 0 ? "primary" : "secondary"
                onClicked: root.mode = 0
            }

            XButton {
                text: I18n.t("dialog.newChat.createGroup", SettingsStore.language) || "Создать группу"
                variant: root.mode === 1 ? "primary" : "secondary"
                onClicked: root.mode = 1
            }
        }

        // Content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spacingLg
            currentIndex: root.mode

            // Find User Mode (0)
            ColumnLayout {
                spacing: Theme.spacingMd

                Text {
                    text: I18n.t("dialog.newChat.usernameHint", SettingsStore.language) || "Введите username пользователя"
                    color: Theme.textSecondary
                    font.pixelSize: 14
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm

                    XTextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "@username"
                        onTextChanged: {
                            root.foundUser = null
                        }
                        Keys.onReturnPressed: {
                            if (text.trim().length > 0) {
                                searchBtn.clicked()
                            }
                        }
                    }

                    XButton {
                        id: searchBtn
                        text: root.searching 
                            ? (I18n.t("dialog.newChat.searching", SettingsStore.language) || "Поиск...")
                            : (I18n.t("dialog.newChat.search", SettingsStore.language) || "Найти")
                        enabled: searchField.text.trim().length > 0 && !root.searching
                        onClicked: {
                            root.searching = true
                            var query = searchField.text.trim()
                            if (query.startsWith("@")) {
                                query = query.substring(1)
                            }
                            ShellViewModel.findUser(query)
                        }
                    }
                }

                // Found user result
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    radius: Theme.radiusMd
                    color: Theme.bgTertiary
                    border.color: root.foundUser ? Theme.success : Theme.borderColor
                    border.width: root.foundUser ? 2 : 1
                    visible: root.foundUser !== null || (searchField.text.length > 0 && !root.searching)

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        spacing: Theme.spacingMd
                        visible: root.foundUser !== null

                        XAvatar {
                            size: 48
                            text: root.foundUser ? root.foundUser.username.charAt(0).toUpperCase() : "?"
                            imageUrl: root.foundUser ? root.foundUser.avatarUrl : ""
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 4

                            Row {
                                spacing: Theme.spacingSm
                                Text {
                                    text: root.foundUser ? ("@" + root.foundUser.username) : ""
                                    color: Theme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: "⭐"
                                    visible: root.foundUser && root.foundUser.isPremium
                                    font.pixelSize: 14
                                }
                            }

                            Text {
                                text: I18n.t("dialog.newChat.userFound", SettingsStore.language) || "Пользователь найден"
                                color: Theme.success
                                font.pixelSize: 13
                            }
                        }

                        XButton {
                            text: I18n.t("dialog.newChat.startChat", SettingsStore.language) || "Начать чат"
                            onClicked: {
                                if (root.foundUser && root.foundUser.userId) {
                                    ShellViewModel.selectChat(root.foundUser.userId)
                                    root.chatStarted(root.foundUser.userId)
                                    root.close()
                                }
                            }
                        }
                    }

                    // No user found
                    Text {
                        anchors.centerIn: parent
                        text: I18n.t("dialog.newChat.notFound", SettingsStore.language) || "Пользователь не найден"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        visible: root.foundUser === null && searchField.text.length > 0 && !root.searching
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Create Group Mode (1)
            ColumnLayout {
                spacing: Theme.spacingMd

                Text {
                    text: I18n.t("dialog.createGroup.nameHint", SettingsStore.language) || "Название группы"
                    color: Theme.textSecondary
                    font.pixelSize: 14
                }

                XTextField {
                    id: groupNameField
                    Layout.fillWidth: true
                    placeholderText: I18n.t("dialog.createGroup.namePlaceholder", SettingsStore.language) || "Введите название..."
                }

                Text {
                    text: I18n.t("dialog.createGroup.membersHint", SettingsStore.language) || "Добавьте участников"
                    color: Theme.textSecondary
                    font.pixelSize: 14
                    Layout.topMargin: Theme.spacingSm
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radiusMd
                    color: Theme.bgTertiary
                    border.color: Theme.borderColor

                    Text {
                        anchors.centerIn: parent
                        text: I18n.t("dialog.createGroup.selectFromChats", SettingsStore.language) || "Выберите из существующих чатов"
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }
                }

                XButton {
                    Layout.alignment: Qt.AlignRight
                    text: I18n.t("dialog.createGroup.create", SettingsStore.language) || "Создать группу"
                    enabled: groupNameField.text.trim().length > 0
                    onClicked: {
                        ShellViewModel.createGroup(groupNameField.text.trim(), [])
                    }
                }
            }
        }
    }
}

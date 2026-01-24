import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../components"
import "../i18n/Translations.js" as I18n

// Telegram-style main view
Item {
    id: root
    signal openSettings()

    // Main background - Telegram style (solid color)
    Rectangle {
        anchors.fill: parent
        color: Theme.bgChat
    }

    // Toast notifications
    XToast {
        id: toast
        anchors.fill: parent
        z: 1000
    }

    Connections {
        target: ShellViewModel
        function onToastRequested(type, message) {
            toast.show(type, message)
        }
    }

    // Main layout - 2 column (sidebar + chat)
    Row {
        anchors.fill: parent

        // Left Sidebar - Chat List
        XChatSidebar {
            id: chatSidebar
            height: parent.height
            width: Theme.sidebarWidth
            
            currentUserName: Session.username || ""
            currentUserAvatar: ""
            selectedChatId: ShellViewModel.selectedChatId
            model: ShellViewModel.chatListModel
            loading: ShellViewModel.loadingChats
            searchQuery: ShellViewModel.searchQuery

            onChatSelected: function(chatId) {
                ShellViewModel.selectChat(chatId)
            }
            onNewChatRequested: newChatDialog.open()
            onSettingsRequested: root.openSettings()
            onProfileRequested: profileDialog.open()
            onSearchChanged: function(query) {
                ShellViewModel.searchQuery = query
            }
        }

        // Main Chat Area
        Item {
            id: chatArea
            width: parent.width - Theme.sidebarWidth - (infoPanelVisible ? Theme.infoPanelWidth : 0)
            height: parent.height

            property bool infoPanelVisible: ShellViewModel.selectedChatId.length > 0

            Behavior on width {
                NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
            }

            // Empty state - Telegram style
            Item {
                anchors.fill: parent
                visible: ShellViewModel.selectedChatId.length === 0

                Column {
                    anchors.centerIn: parent
                    spacing: Theme.spacingMd

                    Rectangle {
                        width: 100
                        height: 100
                        radius: 50
                        color: Theme.bgTertiary
                        anchors.horizontalCenter: parent.horizontalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "💬"
                            font.pixelSize: 48
                            opacity: 0.5
                        }
                    }

                    Text {
                        text: I18n.t("chat.selectPrompt", SettingsStore.language)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamilyDisplay
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: I18n.t("chat.selectHint", SettingsStore.language)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                        horizontalAlignment: Text.AlignHCenter
                        width: 300
                        wrapMode: Text.Wrap
                    }
                }
            }

            // Chat content
            Column {
                anchors.fill: parent
                visible: ShellViewModel.selectedChatId.length > 0

                // Chat Header
                XChatHeader {
                    width: parent.width
                    title: ShellViewModel.selectedChatTitle
                    subtitle: ShellViewModel.typingIndicator.length > 0 
                        ? "" 
                        : (ShellViewModel.selectedChatOnline 
                            ? I18n.t("status.online", SettingsStore.language) 
                            : I18n.t("status.offline", SettingsStore.language))
                    avatarText: ShellViewModel.selectedChatTitle.length > 0 
                        ? ShellViewModel.selectedChatTitle.charAt(0).toUpperCase() 
                        : ""
                    online: ShellViewModel.selectedChatOnline
                    isTyping: ShellViewModel.typingIndicator.length > 0
                    typingText: ShellViewModel.typingIndicator

                    onCallRequested: callStub.open()
                    onVideoCallRequested: callStub.open()
                    onSearchRequested: searchInChatDialog.open()
                    onMenuRequested: chatMenuPopup.open()
                    onInfoRequested: {} // Info panel is always visible
                }

                // Error/Offline banner
                XBanner {
                    width: parent.width
                    text: ShellViewModel.offline 
                        ? I18n.t("banner.offline", SettingsStore.language) 
                        : (ShellViewModel.errorMessage === "Server error" 
                            ? I18n.t("banner.serverError", SettingsStore.language) 
                            : ShellViewModel.errorMessage)
                    variant: "error"
                    visible: ShellViewModel.offline || ShellViewModel.errorMessage.length > 0
                }

                // Messages area
                Item {
                    width: parent.width
                    height: parent.height - 64 - messageInputArea.height - (bannerVisible ? 44 : 0)
                    
                    property bool bannerVisible: ShellViewModel.offline || ShellViewModel.errorMessage.length > 0

                    // Messages list
                    ListView {
                        id: messageList
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        clip: true
                        spacing: 4
                        verticalLayoutDirection: ListView.BottomToTop
                        
                        model: ShellViewModel.messageListModel
                        
                        delegate: XMessageBubble {
                            width: messageList.width
                            content: model.content
                            sent: model.sent
                            time: model.time
                            status: model.status
                            messageId: model.id
                            tempId: model.tempId
                            messageType: model.messageType
                            fileName: model.fileName
                            filePath: model.filePath
                            localFilePath: model.localFilePath
                            transferState: model.transferState
                            transferProgress: model.transferProgress
                            senderName: model.senderName || ""
                            senderAvatar: model.senderAvatar || ""
                            showAvatar: !model.sent
                            isLast: index === 0
                            
                            onCopyRequested: function(text) {
                                ShellViewModel.copyToClipboard(text)
                                toast.show("info", I18n.t("toast.copied", SettingsStore.language))
                            }
                            onDeleteRequested: function(msgId) {
                                ShellViewModel.deleteMessage(msgId)
                            }
                            onRetryRequested: function(msgId) {
                                ShellViewModel.retryMessage(msgId)
                            }
                            onReplyRequested: {
                                messageInputArea.setReply(model.senderName || ShellViewModel.selectedChatTitle, model.content)
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        // Scroll to bottom on new messages
                        onCountChanged: {
                            if (atYEnd || count <= 1) {
                                positionViewAtBeginning()
                            }
                        }
                    }

                    // Loading state
                    Column {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingMd
                        spacing: Theme.spacingSm
                        visible: ShellViewModel.loadingMessages

                        Repeater {
                            model: 5
                            delegate: Rectangle {
                                height: 52
                                width: parent.width * (0.4 + Math.random() * 0.3)
                                anchors.right: index % 2 === 0 ? parent.right : undefined
                                radius: 18
                                color: Theme.bgTertiary
                                opacity: 0.5

                                SequentialAnimation on opacity {
                                    running: ShellViewModel.loadingMessages
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.2; duration: 800; easing.type: Easing.InOutQuad }
                                    NumberAnimation { to: 0.5; duration: 800; easing.type: Easing.InOutQuad }
                                }
                            }
                        }
                    }

                    // Empty messages state
                    Column {
                        anchors.centerIn: parent
                        spacing: Theme.spacingMd
                        visible: !ShellViewModel.loadingMessages && messageList.count === 0

                        Text {
                            text: "✉️"
                            font.pixelSize: 48
                            anchors.horizontalCenter: parent.horizontalCenter
                            opacity: 0.5
                        }

                        Text {
                            text: I18n.t("chat.noMessages", SettingsStore.language)
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

                    // Drop area for files
                    DropArea {
                        id: dropArea
                        anchors.fill: parent
                        
                        onDropped: function(drop) {
                            for (var i = 0; i < drop.urls.length; ++i) {
                                ShellViewModel.addAttachment(drop.urls[i].toString().replace("file://", ""))
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(Theme.purplePrimary.r, Theme.purplePrimary.g, Theme.purplePrimary.b, 0.15)
                            border.color: Theme.accentSoft
                            border.width: 2
                            radius: Theme.radiusMd
                            visible: dropArea.containsDrag

                            Column {
                                anchors.centerIn: parent
                                spacing: Theme.spacingMd

                                Text {
                                    text: "📎"
                                    font.pixelSize: 48
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: I18n.t("chat.dropFiles", SettingsStore.language)
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 14
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }
                        }
                    }
                }

                // Upload progress
                Flow {
                    width: parent.width - Theme.spacingLg * 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.spacingSm
                    visible: ShellViewModel.uploadListModel.rowCount() > 0

                    Repeater {
                        model: ShellViewModel.uploadListModel
                        delegate: XUploadItem {
                            tempId: model.tempId
                            fileName: model.fileName
                            progress: model.progress
                            uploadState: model.state
                            errorMessage: model.errorMessage
                            onCancelRequested: ShellViewModel.cancelUpload(tempId)
                        }
                    }
                }

                // Message input area
                XMessageInput {
                    id: messageInputArea
                    width: parent.width
                    canSend: !ShellViewModel.offline && ShellViewModel.selectedChatId.length > 0

                    property string replyToAuthorValue: ""
                    property string replyToTextValue: ""

                    replyToText: replyToTextValue
                    replyToAuthor: replyToAuthorValue

                    function setReply(author, text) {
                        replyToAuthorValue = author
                        replyToTextValue = text.length > 100 ? text.substring(0, 100) + "..." : text
                    }

                    onSendRequested: function(message) {
                        if (message.trim().length > 0) {
                            ShellViewModel.sendMessage(message)
                            replyToAuthorValue = ""
                            replyToTextValue = ""
                        }
                    }

                    onAttachmentRequested: FileDialogService.openFiles()
                    
                    onEmojiRequested: emojiPicker.visible = !emojiPicker.visible
                    
                    onVoiceRequested: {
                        toast.show("info", I18n.t("toast.voiceNotSupported", SettingsStore.language))
                    }

                    onCancelReply: {
                        replyToAuthorValue = ""
                        replyToTextValue = ""
                    }

                    onTextChanged: {
                        ShellViewModel.sendTyping(true)
                        typingTimer.restart()
                    }
                }
            }
        }

        // Right Info Panel
        XInfoPanel {
            id: infoPanel
            height: parent.height
            width: Theme.infoPanelWidth
            visible: ShellViewModel.selectedChatId.length > 0

            chatTitle: ShellViewModel.selectedChatTitle
            chatAvatarText: ShellViewModel.selectedChatTitle.length > 0 
                ? ShellViewModel.selectedChatTitle.charAt(0).toUpperCase() 
                : ""
            chatOnline: ShellViewModel.selectedChatOnline

            onSettingsRequested: root.openSettings()
            onMuteToggled: toast.show("info", I18n.t("toast.notImplemented", SettingsStore.language))
            onPinToggled: toast.show("info", I18n.t("toast.notImplemented", SettingsStore.language))
            onClearHistoryRequested: clearHistoryDialog.open()
            onDeleteChatRequested: deleteChatDialog.open()
        }
    }

    // Emoji picker popup
    Popup {
        id: emojiPicker
        x: parent.width - 420
        y: parent.height - 400
        width: 360
        height: 340
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Theme.bgSecondary
            border.color: Theme.borderColor
            radius: Theme.radiusLg

            Rectangle {
                anchors.fill: parent
                anchors.margins: -1
                radius: parent.radius + 1
                color: "transparent"
                opacity: 0.3
                z: -1
            }
        }

        contentItem: Column {
            spacing: Theme.spacingSm
            padding: Theme.spacingMd

            // Header
            Row {
                width: parent.width - Theme.spacingMd * 2
                
                Text {
                    text: I18n.t("emoji.title", SettingsStore.language)
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Item { width: parent.width - 100; height: 1 }

                XIconButton {
                    size: 28
                    iconText: "✕"
                    variant: "ghost"
                    onClicked: emojiPicker.close()
                }
            }

            // Search
            XTextField {
                width: parent.width - Theme.spacingMd * 2
                height: 36
                placeholderText: I18n.t("emoji.search", SettingsStore.language)
            }

            // Emoji grid
            GridView {
                width: parent.width - Theme.spacingMd * 2
                height: 220
                cellWidth: 40
                cellHeight: 40
                clip: true

                model: ["😀", "😃", "😄", "😁", "😅", "😂", "🤣", "😊", 
                        "😇", "🙂", "🙃", "😉", "😌", "😍", "🥰", "😘",
                        "😗", "😙", "😚", "😋", "😛", "😜", "🤪", "😝",
                        "🤑", "🤗", "🤭", "🤫", "🤔", "🤐", "🤨", "😐",
                        "😑", "😶", "😏", "😒", "🙄", "😬", "🤥", "😌",
                        "👍", "👎", "👌", "✌️", "🤞", "🤟", "🤘", "🤙",
                        "❤️", "🧡", "💛", "💚", "💙", "💜", "🖤", "🤍"]

                delegate: Rectangle {
                    width: 36
                    height: 36
                    radius: Theme.radiusSm
                    color: emojiMouse.containsMouse ? Theme.bgTertiary : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: 22
                    }

                    MouseArea {
                        id: emojiMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            messageInputArea.text += modelData
                            emojiPicker.close()
                        }
                    }
                }
            }
        }
    }

    // Typing timer
    Timer {
        id: typingTimer
        interval: 1500
        repeat: false
        onTriggered: ShellViewModel.sendTyping(false)
    }

    // File dialog connection
    Connections {
        target: FileDialogService
        function onFilesSelected(files) {
            for (var i = 0; i < files.length; ++i) {
                ShellViewModel.addAttachment(files[i])
            }
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: StandardKey.Find
        onActivated: searchInChatDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: chatSidebar.searchQuery = ""
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (emojiPicker.visible) emojiPicker.close()
        }
    }

    // Dialogs and popups
    CallStub {
        id: callStub
    }

    NewChatDialog {
        id: newChatDialog
        
        onChatStarted: function(chatId) {
            ShellViewModel.selectChat(chatId)
        }
    }

    XFeatureStub {
        id: profileDialog
        titleText: I18n.t("dialog.profile.title", SettingsStore.language)
        bodyText: I18n.t("dialog.profile.body", SettingsStore.language)
    }

    XFeatureStub {
        id: searchInChatDialog
        titleText: I18n.t("dialog.search.title", SettingsStore.language)
        bodyText: I18n.t("dialog.search.body", SettingsStore.language)
    }

    XFeatureStub {
        id: clearHistoryDialog
        titleText: I18n.t("dialog.clearHistory.title", SettingsStore.language)
        bodyText: I18n.t("dialog.clearHistory.body", SettingsStore.language)
    }

    XFeatureStub {
        id: deleteChatDialog
        titleText: I18n.t("dialog.deleteChat.title", SettingsStore.language)
        bodyText: I18n.t("dialog.deleteChat.body", SettingsStore.language)
    }

    Popup {
        id: chatMenuPopup
        x: parent.width - Theme.sidebarWidth - 180
        y: 70
        width: 160
        padding: Theme.spacingSm

        background: Rectangle {
            color: Theme.bgSecondary
            border.color: Theme.borderColor
            radius: Theme.radiusMd
        }

        Column {
            width: parent.width
            spacing: 2

            Repeater {
                model: [
                    { icon: "🔍", text: I18n.t("menu.search", SettingsStore.language) },
                    { icon: "📌", text: I18n.t("menu.pin", SettingsStore.language) },
                    { icon: "🔇", text: I18n.t("menu.mute", SettingsStore.language) },
                    { icon: "🗑️", text: I18n.t("menu.clear", SettingsStore.language) }
                ]

                delegate: Rectangle {
                    width: parent.width
                    height: 36
                    radius: Theme.radiusSm
                    color: menuItemMouse.containsMouse ? Theme.bgTertiary : "transparent"

                    Row {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingSm

                        Text {
                            text: modelData.icon
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: modelData.text
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: menuItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            chatMenuPopup.close()
                            toast.show("info", I18n.t("toast.notImplemented", SettingsStore.language))
                        }
                    }
                }
            }
        }
    }
}

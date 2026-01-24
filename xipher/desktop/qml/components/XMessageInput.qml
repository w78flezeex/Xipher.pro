import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Xipher.Desktop
import "../i18n/Translations.js" as I18n

// Telegram-style message input
Rectangle {
    id: root

    property alias text: messageInput.text
    property bool canSend: false
    property bool showAttachments: true
    property string replyToText: ""
    property string replyToAuthor: ""
    property var pendingFiles: []

    signal sendRequested(string message)
    signal attachmentRequested()
    signal emojiRequested()
    signal voiceRequested()
    signal cancelReply()
    signal removeFile(int index)
    signal textChanged()

    height: contentColumn.implicitHeight
    color: Theme.bgHeader

    // Top border
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.borderColor
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        spacing: Theme.spacingSm

        // Reply preview - Telegram style
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: 6
            color: Theme.bgTertiary
            visible: root.replyToText.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm

                Rectangle {
                    width: 2
                    Layout.fillHeight: true
                    Layout.margins: 8
                    radius: 1
                    color: Theme.primary
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 2

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

                XIconButton {
                    size: 28
                    iconText: "✕"
                    variant: "ghost"
                    onClicked: root.cancelReply()
                }
            }
        }

        // Pending files - Telegram style
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            visible: root.pendingFiles.length > 0

            Repeater {
                model: root.pendingFiles
                delegate: Rectangle {
                    width: Math.min(200, implicitWidth)
                    implicitWidth: fileRow.implicitWidth + Theme.spacingMd
                    height: 48
                    radius: 6
                    color: Theme.bgTertiary

                    Row {
                        id: fileRow
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingSm

                        Rectangle {
                            width: 32
                            height: 32
                            radius: 6
                            color: Theme.primary
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: modelData.isImage ? "🖼️" : "📄"
                                font.pixelSize: 14
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 120

                            Text {
                                text: modelData.name
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                width: parent.width
                            }

                            Text {
                                text: modelData.size
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                            }
                        }

                        XIconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            size: 22
                            iconText: "✕"
                            variant: "ghost"
                            onClicked: root.removeFile(index)
                        }
                    }
                }
            }
        }

        // Input area - Telegram style
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(48, messageInput.implicitHeight + 16)
            spacing: Theme.spacingXs

            // Attachment button
            XIconButton {
                size: 40
                iconText: "📎"
                variant: "ghost"
                visible: root.showAttachments
                onClicked: root.attachmentRequested()
                ToolTip.text: I18n.t("chat.attach", SettingsStore.language)
                ToolTip.visible: hovered
            }

            // Text input - Telegram style (no border, flat)
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 20
                color: Theme.bgTertiary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 6
                    spacing: 4

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        TextArea {
                            id: messageInput
                            placeholderText: I18n.t("chat.compose", SettingsStore.language)
                            wrapMode: TextArea.Wrap
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textMuted
                            background: Item {}
                            
                            Keys.onPressed: function(event) {
                                if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ShiftModifier)) {
                                    if (root.canSend || messageInput.text.trim().length > 0) {
                                        root.sendRequested(messageInput.text)
                                        messageInput.text = ""
                                    }
                                    event.accepted = true
                                }
                            }

                            onTextChanged: root.textChanged()
                        }
                    }

                    // Emoji button
                    XIconButton {
                        size: 32
                        iconText: "😊"
                        variant: "ghost"
                        onClicked: root.emojiRequested()
                    }
                }
            }

            // Voice/Send button - Telegram style (round)
            Rectangle {
                width: 44
                height: 44
                radius: 22
                color: messageInput.text.length > 0 ? Theme.primary : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: messageInput.text.length > 0 ? "➤" : "🎤"
                    font.pixelSize: 18
                    color: messageInput.text.length > 0 ? "#FFFFFF" : Theme.textSecondary
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (messageInput.text.length > 0) {
                            root.sendRequested(messageInput.text)
                            messageInput.text = ""
                        } else {
                            root.voiceRequested()
                        }
                    }
                }
            }
        }
    }
}

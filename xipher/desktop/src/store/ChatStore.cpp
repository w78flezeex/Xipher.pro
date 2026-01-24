#include "store/ChatStore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUuid>

#include "logging/Log.h"

namespace {
QString formatUploadLimit(qint64 bytes) {
    if (bytes <= 0) {
        return QString();
    }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    const int decimals = mb < 10.0 ? 1 : 0;
    return QString::number(mb, 'f', decimals);
}

QString resolveRemoteUrl(ApiClient* api, const QString& url) {
    if (!api) {
        return url;
    }
    return api->resolveUrl(url);
}
}  // namespace

ChatStore::ChatStore(ApiClient* api, WsClient* ws, UploadManager* uploads, Session* session, QObject* parent)
    : QObject(parent), api_(api), ws_(ws), uploads_(uploads), session_(session) {
    typingClearTimer_.setSingleShot(true);
    typingClearTimer_.setInterval(3000);
    connect(&typingClearTimer_, &QTimer::timeout, this, [this]() {
        typingIndicator_.clear();
        emit typingIndicatorChanged();
    });

    if (api_) {
        connect(api_, &ApiClient::chatsFetched, this,
                [this](const ApiDtos::ChatListResponse& response) { handleChatsResponse(response); });
        connect(api_, &ApiClient::messagesFetched, this,
                [this](const QString& chatId, const ApiDtos::MessageListResponse& response) {
                    handleMessagesResponse(chatId, response);
                });
        connect(api_, &ApiClient::messageSent, this,
                [this](const QString& chatId, const ApiDtos::SendMessageResponse& response) {
                    if (!response.success) {
                        const QString msg = response.error.userMessage.isEmpty() ? response.message
                                                                                 : response.error.userMessage;
                        setErrorMessage(msg);
                        emit toastRequested("error", msg);
                        if (!response.messageData.tempId.isEmpty()) {
                            messageListModel_.updateMessageStatus(response.messageData.tempId, "failed", false, false);
                        }
                        return;
                    }

                    Message msg = response.messageData;
                    msg.sent = true;
                    msg.receiverId = chatId;
                    messageListModel_.reconcileTempMessage(msg.tempId, msg);

                    Chat chat = chatListModel_.chatById(chatId);
                    if (!chat.id.isEmpty()) {
                        chat.lastMessage = msg.content;
                        chat.time = msg.time;
                        chatListModel_.updateChat(chat);
                    }
                    setErrorMessage(QString());
                });
        connect(api_, &ApiClient::messageDeleted, this,
                [this](const QString& messageId, const ApiDtos::ApiError& error) {
                    if (!error.userMessage.isEmpty()) {
                        emit toastRequested("error", error.userMessage);
                        return;
                    }
                    messageListModel_.removeMessage(messageId);
                });
        connect(api_, &ApiClient::userFound, this, [this](const ApiDtos::FindUserResponse& resp) {
            emit userFound(resp.success, resp.userId, resp.username, resp.avatarUrl, resp.isPremium, resp.message);
        });
        connect(api_, &ApiClient::groupCreated, this, [this](const ApiDtos::CreateGroupResponse& resp) {
            emit groupCreated(resp.success, resp.groupId, resp.message);
            if (resp.success) {
                refreshChats();
            }
        });
    }

    if (ws_) {
        connect(ws_, &WsClient::connectionStateChanged, this, [this](bool connected) {
            setOffline(!connected);
        });
        connect(ws_, &WsClient::newMessage, this, [this](const Message& msg) { handleNewMessage(msg); });
        connect(ws_, &WsClient::messageDeleted, this, [this](const QString& messageId, const QString&) {
            messageListModel_.removeMessage(messageId);
        });
        connect(ws_, &WsClient::typingReceived, this, [this](const TypingEvent& event) {
            if (event.chatId.isEmpty() || event.fromUserId.isEmpty()) {
                return;
            }
            if (event.chatId != selectedChatId_) {
                return;
            }
            if (event.isTyping) {
                typingIndicator_ = event.fromUsername.isEmpty() ? "Typing..." : (event.fromUsername + " is typing...");
                emit typingIndicatorChanged();
                typingClearTimer_.start();
            } else {
                typingIndicator_.clear();
                emit typingIndicatorChanged();
            }
        });
        connect(ws_, &WsClient::messageDelivered, this, [this](const QString& messageId, const QString&,
                                                             const QString&) {
            messageListModel_.updateMessageStatus(messageId, "delivered", false, true);
        });
        connect(ws_, &WsClient::messageRead, this, [this](const QString& messageId, const QString&,
                                                         const QString&) {
            messageListModel_.updateMessageStatus(messageId, "read", true, true);
        });
    }

    if (uploads_) {
        connect(uploads_, &UploadManager::uploadProgress, this, [this](const QString& tempId, int progress) {
            uploadListModel_.updateProgress(tempId, progress);
            messageListModel_.updateMessageTransfer(tempId, "uploading", progress);
        });
        connect(uploads_, &UploadManager::uploadFinished, this,
                [this](const QString& tempId, const ApiDtos::UploadFileResponse& response,
                       const QString& messageType) { handleUploadFinished(tempId, response, messageType); });
        connect(uploads_, &UploadManager::uploadFailed, this,
                [this](const QString& tempId, const ApiDtos::ApiError& error) { handleUploadFailed(tempId, error); });
        connect(uploads_, &UploadManager::uploadCancelled, this, [this](const QString& tempId) {
            uploadListModel_.removeUpload(tempId);
            messageListModel_.removeMessage(tempId);
            uploadTargets_.remove(tempId);
        });
    }
}

ChatListModel* ChatStore::chatListModel() {
    return &chatListModel_;
}

MessageListModel* ChatStore::messageListModel() {
    return &messageListModel_;
}

UploadListModel* ChatStore::uploadListModel() {
    return &uploadListModel_;
}

QString ChatStore::selectedChatId() const {
    return selectedChatId_;
}

QString ChatStore::selectedChatTitle() const {
    return selectedChatTitle_;
}

bool ChatStore::selectedChatOnline() const {
    return selectedChatOnline_;
}

bool ChatStore::loadingChats() const {
    return loadingChats_;
}

bool ChatStore::loadingMessages() const {
    return loadingMessages_;
}

bool ChatStore::offline() const {
    return offline_;
}

QString ChatStore::errorMessage() const {
    return errorMessage_;
}

QString ChatStore::typingIndicator() const {
    return typingIndicator_;
}

void ChatStore::bootstrapAfterAuth() {
    fetchChatsWithRetry(0);
    if (ws_) {
        ws_->connectToServer();
    }
}

void ChatStore::refreshChats() {
    fetchChatsWithRetry(0);
}

void ChatStore::selectChat(const QString& chatId) {
    if (selectedChatId_ == chatId) {
        return;
    }
    selectedChatId_ = chatId;
    emit selectedChatIdChanged();

    typingIndicator_.clear();
    emit typingIndicatorChanged();

    chatListModel_.clearUnread(chatId);
    updateSelectedChatTitle();

    if (!api_) {
        return;
    }
    loadingMessages_ = true;
    emit loadingMessagesChanged();
    messageListModel_.setMessages({});
    fetchMessagesWithRetry(chatId, 0);
}

void ChatStore::sendMessage(const QString& text, const QString& replyToId) {
    if (selectedChatId_.isEmpty()) {
        emit toastRequested("warning", "Select a chat first.");
        return;
    }
    if (text.trimmed().isEmpty()) {
        return;
    }
    if (offline_) {
        emit toastRequested("error", "You are offline.");
        return;
    }

    const QString tempId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Message msg;
    msg.tempId = tempId;
    msg.senderId = session_ ? session_->userId() : QString();
    msg.receiverId = selectedChatId_;
    msg.content = text.trimmed();
    msg.sent = true;
    msg.status = "sending";
    msg.messageType = "text";
    msg.time = QDateTime::currentDateTime().toString("HH:mm");
    msg.replyToMessageId = replyToId;
    messageListModel_.appendMessage(msg);

    if (api_) {
        ApiClient::SendMessageRequest request;
        request.chatId = selectedChatId_;
        request.content = msg.content;
        request.messageType = msg.messageType;
        request.replyToId = replyToId;
        request.tempId = tempId;
        api_->sendMessage(request);
    }
}

void ChatStore::addAttachment(const QString& filePath) {
    if (!uploads_) {
        return;
    }
    if (selectedChatId_.isEmpty()) {
        emit toastRequested("warning", "Select a chat first.");
        return;
    }
    if (offline_) {
        emit toastRequested("error", "You are offline.");
        return;
    }
    QFileInfo info(filePath);
    if (!info.exists()) {
        emit toastRequested("error", "File not found.");
        return;
    }
    const QString tempId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString fileName = info.fileName();
    const qint64 fileSize = info.size();
    const QString messageType = resolveMessageType(filePath, fileName);
    const qint64 limit = uploads_->maxUploadBytes();
    if (limit > 0 && fileSize > limit) {
        const QString pretty = formatUploadLimit(limit);
        emit toastRequested("error", QString("File too large. Max %1 MB.").arg(pretty));
        return;
    }

    UploadItem uploadItem;
    uploadItem.tempId = tempId;
    uploadItem.filePath = filePath;
    uploadItem.fileName = fileName;
    uploadItem.fileSize = fileSize;
    uploadItem.state = "uploading";
    uploadItem.progress = 0;
    uploadListModel_.addUpload(uploadItem);

    Message msg;
    msg.tempId = tempId;
    msg.senderId = session_ ? session_->userId() : QString();
    msg.receiverId = selectedChatId_;
    msg.content = buildAttachmentContent(messageType, fileName);
    msg.sent = true;
    msg.status = "sending";
    msg.messageType = messageType;
    msg.fileName = fileName;
    msg.fileSize = fileSize;
    msg.localFilePath = filePath;
    msg.transferState = "uploading";
    msg.transferProgress = 0;
    msg.time = QDateTime::currentDateTime().toString("HH:mm");
    messageListModel_.appendMessage(msg);

    UploadManager::UploadRequest request;
    request.tempId = tempId;
    request.filePath = filePath;
    request.fileName = fileName;
    request.fileSize = fileSize;
    request.messageType = messageType;
    uploadTargets_.insert(tempId, selectedChatId_);
    uploads_->startUpload(request);
}

void ChatStore::cancelUpload(const QString& tempId) {
    if (!uploads_) {
        return;
    }
    uploads_->cancelUpload(tempId);
}

void ChatStore::retryMessage(const QString& messageId) {
    Message msg = messageListModel_.messageById(messageId);
    if (msg.id.isEmpty()) {
        msg = messageListModel_.messageByTempId(messageId);
    }
    if (msg.tempId.isEmpty()) {
        return;
    }
    if (msg.messageType == "text") {
        sendMessage(msg.content, msg.replyToMessageId);
        return;
    }
    if (msg.localFilePath.isEmpty()) {
        return;
    }
    if (!uploads_) {
        return;
    }
    uploadTargets_.insert(msg.tempId, msg.receiverId.isEmpty() ? selectedChatId_ : msg.receiverId);
    uploadListModel_.removeUpload(msg.tempId);
    uploadListModel_.addUpload({msg.tempId, msg.localFilePath, msg.fileName, msg.fileSize, "uploading", 0, QString()});
    messageListModel_.updateMessageTransfer(msg.tempId, "uploading", 0);
    messageListModel_.updateMessageStatus(msg.tempId, "sending", false, false);

    UploadManager::UploadRequest request;
    request.tempId = msg.tempId;
    request.filePath = msg.localFilePath;
    request.fileName = msg.fileName;
    request.fileSize = msg.fileSize;
    request.messageType = msg.messageType;
    uploads_->startUpload(request);
}

void ChatStore::deleteMessage(const QString& messageId) {
    if (!api_ || messageId.isEmpty()) {
        return;
    }
    api_->deleteMessage(messageId);
}

void ChatStore::setSearchQuery(const QString& query) {
    chatListModel_.setFilterText(query);
}

void ChatStore::sendTyping(bool isTyping) {
    if (!ws_ || selectedChatId_.isEmpty()) {
        return;
    }
    ws_->sendTyping(selectedChatId_, "chat", isTyping);
}

void ChatStore::fetchChatsWithRetry(int attempt) {
    if (!api_) {
        return;
    }
    chatFetchAttempt_ = attempt;
    loadingChats_ = true;
    emit loadingChatsChanged();
    api_->fetchChats();
}

void ChatStore::fetchMessagesWithRetry(const QString& chatId, int attempt) {
    if (!api_) {
        return;
    }
    messageFetchAttempts_[chatId] = attempt;
    loadingMessages_ = true;
    emit loadingMessagesChanged();
    api_->fetchMessages(chatId);
}

void ChatStore::handleChatsResponse(const ApiDtos::ChatListResponse& response) {
    loadingChats_ = false;
    emit loadingChatsChanged();

    if (!response.success) {
        QString msg = response.error.userMessage.isEmpty() ? response.message : response.error.userMessage;
        // Make error message more user-friendly
        if (msg == "Invalid JSON response") {
            msg = "Server error";
        }
        setErrorMessage(msg.isEmpty() ? "Failed to load chats" : msg);
        emit toastRequested("error", errorMessage_);
        if (response.error.isNetworkError) {
            setOffline(true);
        }
        if (response.error.isTransient && chatFetchAttempt_ < 2) {
            const int nextAttempt = chatFetchAttempt_ + 1;
            const int delayMs = 1000 * (1 << chatFetchAttempt_);
            QTimer::singleShot(delayMs, this, [this, nextAttempt]() { fetchChatsWithRetry(nextAttempt); });
        }
        return;
    }

    setOffline(false);
    chatFetchAttempt_ = 0;
    setErrorMessage(QString());
    QVector<Chat> chats = response.chats;
    for (Chat& chat : chats) {
        chat.avatarUrl = resolveRemoteUrl(api_, chat.avatarUrl);
    }
    chatListModel_.setChats(chats);
    updateSelectedChatTitle();
}

void ChatStore::handleMessagesResponse(const QString& chatId, const ApiDtos::MessageListResponse& response) {
    loadingMessages_ = false;
    emit loadingMessagesChanged();

    if (!response.success) {
        QString msg = response.error.userMessage.isEmpty() ? response.message : response.error.userMessage;
        // Make error message more user-friendly
        if (msg == "Invalid JSON response") {
            msg = "Server error";
        }
        setErrorMessage(msg.isEmpty() ? "Failed to load messages" : msg);
        emit toastRequested("error", errorMessage_);
        if (response.error.isNetworkError) {
            setOffline(true);
        }
        const int attempt = messageFetchAttempts_.value(chatId, 0);
        if (response.error.isTransient && attempt < 2) {
            const int nextAttempt = attempt + 1;
            const int delayMs = 1000 * (1 << attempt);
            QTimer::singleShot(delayMs, this, [this, chatId, nextAttempt]() {
                fetchMessagesWithRetry(chatId, nextAttempt);
            });
        }
        return;
    }

    setOffline(false);
    messageFetchAttempts_.remove(chatId);
    setErrorMessage(QString());
    QVector<Message> messages = response.messages;
    for (Message& msg : messages) {
        msg.filePath = resolveRemoteUrl(api_, msg.filePath);
    }
    messageListModel_.setMessages(messages);

    if (!response.messages.isEmpty()) {
        const Message lastMsg = response.messages.last();
        if (!lastMsg.sent && ws_) {
            ws_->sendReceipt("message_read", lastMsg.id);
        }
    }
}

void ChatStore::handleUploadFinished(const QString& tempId, const ApiDtos::UploadFileResponse& response,
                                     const QString& messageType) {
    uploadListModel_.removeUpload(tempId);
    if (!response.success) {
        handleUploadFailed(tempId, response.error);
        return;
    }
    const QString chatId = uploadTargets_.value(tempId);
    uploadTargets_.remove(tempId);
    if (chatId.isEmpty()) {
        return;
    }
    ApiClient::SendMessageRequest request;
    request.chatId = chatId;
    request.messageType = messageType;
    request.filePath = response.filePath;
    request.fileName = response.fileName;
    request.fileSize = response.fileSize;
    request.content = buildAttachmentContent(messageType, response.fileName);
    request.tempId = tempId;
    if (api_) {
        api_->sendMessage(request);
    }
}

void ChatStore::handleUploadFailed(const QString& tempId, const ApiDtos::ApiError& error) {
    uploadListModel_.updateState(tempId, "failed", error.userMessage);
    messageListModel_.updateMessageTransfer(tempId, "failed", 0);
    messageListModel_.updateMessageStatus(tempId, "failed", false, false);
    uploadTargets_.remove(tempId);
    const QString msg = error.userMessage.isEmpty() ? "Upload failed" : error.userMessage;
    emit toastRequested("error", msg);
}

void ChatStore::setErrorMessage(const QString& message) {
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged();
}

void ChatStore::setOffline(bool offline) {
    if (offline_ == offline) {
        return;
    }
    offline_ = offline;
    emit offlineChanged();
}

void ChatStore::updateSelectedChatTitle() {
    if (selectedChatId_.isEmpty()) {
        selectedChatTitle_.clear();
        emit selectedChatTitleChanged();
        if (selectedChatOnline_) {
            selectedChatOnline_ = false;
            emit selectedChatOnlineChanged();
        }
        return;
    }
    const Chat chat = chatListModel_.chatById(selectedChatId_);
    const QString title = chat.displayName.isEmpty() ? chat.name : chat.displayName;
    if (selectedChatTitle_ != title) {
        selectedChatTitle_ = title;
        emit selectedChatTitleChanged();
    }
    if (selectedChatOnline_ != chat.online) {
        selectedChatOnline_ = chat.online;
        emit selectedChatOnlineChanged();
    }
}

QString ChatStore::buildAttachmentContent(const QString& messageType, const QString& fileName) const {
    if (messageType == "image") {
        return "Photo";
    }
    if (messageType == "voice") {
        return "Voice message";
    }
    const QString name = fileName.isEmpty() ? "file" : fileName;
    return "File: " + name;
}

QString ChatStore::resolveMessageType(const QString& filePath, const QString& fileName) const {
    Q_UNUSED(fileName);
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(filePath);
    if (mime.isValid() && mime.name().startsWith("image/")) {
        return "image";
    }
    return "file";
}

void ChatStore::handleNewMessage(const Message& message) {
    if (message.id.isEmpty()) {
        return;
    }

    Message resolved = message;
    resolved.filePath = resolveRemoteUrl(api_, resolved.filePath);

    const QString chatId = !resolved.chatId.isEmpty() ? resolved.chatId
                                                      : (resolved.sent ? resolved.receiverId : resolved.senderId);

    Chat chat = chatListModel_.chatById(chatId);
    if (!chat.id.isEmpty()) {
        chat.lastMessage = resolved.content;
        chat.time = resolved.time;
        chatListModel_.updateChat(chat);
    } else if (!chatId.isEmpty()) {
        refreshChats();
    }

    if (chatId == selectedChatId_) {
        const Message existing = messageListModel_.messageById(resolved.id);
        if (resolved.sent && !resolved.tempId.isEmpty()) {
            messageListModel_.reconcileTempMessage(resolved.tempId, resolved);
        } else if (existing.id.isEmpty()) {
            messageListModel_.appendMessage(resolved);
        }
        if (!resolved.sent && ws_) {
            ws_->sendReceipt("message_read", resolved.id);
        }
    } else {
        chatListModel_.incrementUnread(chatId);
        if (!resolved.sent && ws_) {
            ws_->sendReceipt("message_delivered", resolved.id);
        }
    }
}

void ChatStore::findUser(const QString& username) {
    if (!api_) {
        emit userFound(false, QString(), QString(), QString(), false, "API not available");
        return;
    }
    api_->findUser(username);
}

void ChatStore::createGroup(const QString& name, const QStringList& memberIds) {
    if (!api_) {
        emit groupCreated(false, QString(), "API not available");
        return;
    }
    api_->createGroup(name, memberIds);
}

#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>

#include "app/Session.h"
#include "net/ApiClient.h"
#include "net/UploadManager.h"
#include "net/WsClient.h"
#include "store/ChatListModel.h"
#include "store/MessageListModel.h"
#include "store/UploadListModel.h"

class ChatStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(ChatListModel* chatListModel READ chatListModel CONSTANT)
    Q_PROPERTY(MessageListModel* messageListModel READ messageListModel CONSTANT)
    Q_PROPERTY(UploadListModel* uploadListModel READ uploadListModel CONSTANT)
    Q_PROPERTY(QString selectedChatId READ selectedChatId NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedChatTitle READ selectedChatTitle NOTIFY selectedChatTitleChanged)
    Q_PROPERTY(bool selectedChatOnline READ selectedChatOnline NOTIFY selectedChatOnlineChanged)
    Q_PROPERTY(bool loadingChats READ loadingChats NOTIFY loadingChatsChanged)
    Q_PROPERTY(bool loadingMessages READ loadingMessages NOTIFY loadingMessagesChanged)
    Q_PROPERTY(bool offline READ offline NOTIFY offlineChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString typingIndicator READ typingIndicator NOTIFY typingIndicatorChanged)

public:
    explicit ChatStore(ApiClient* api, WsClient* ws, UploadManager* uploads, Session* session,
                       QObject* parent = nullptr);

    ChatListModel* chatListModel();
    MessageListModel* messageListModel();
    UploadListModel* uploadListModel();

    QString selectedChatId() const;
    QString selectedChatTitle() const;
    bool selectedChatOnline() const;
    bool loadingChats() const;
    bool loadingMessages() const;
    bool offline() const;
    QString errorMessage() const;
    QString typingIndicator() const;

    Q_INVOKABLE void bootstrapAfterAuth();
    Q_INVOKABLE void refreshChats();
    Q_INVOKABLE void selectChat(const QString& chatId);
    Q_INVOKABLE void sendMessage(const QString& text, const QString& replyToId = QString());
    Q_INVOKABLE void addAttachment(const QString& filePath);
    Q_INVOKABLE void cancelUpload(const QString& tempId);
    Q_INVOKABLE void retryMessage(const QString& messageId);
    Q_INVOKABLE void deleteMessage(const QString& messageId);
    Q_INVOKABLE void setSearchQuery(const QString& query);
    Q_INVOKABLE void sendTyping(bool isTyping);
    Q_INVOKABLE void findUser(const QString& username);
    Q_INVOKABLE void createGroup(const QString& name, const QStringList& memberIds);

signals:
    void selectedChatIdChanged();
    void selectedChatTitleChanged();
    void selectedChatOnlineChanged();
    void loadingChatsChanged();
    void loadingMessagesChanged();
    void offlineChanged();
    void errorMessageChanged();
    void typingIndicatorChanged();
    void toastRequested(const QString& type, const QString& message);
    void userFound(bool success, const QString& userId, const QString& username,
                   const QString& avatarUrl, bool isPremium, const QString& message);
    void groupCreated(bool success, const QString& groupId, const QString& message);

private:
    void handleChatsResponse(const ApiDtos::ChatListResponse& response);
    void handleMessagesResponse(const QString& chatId, const ApiDtos::MessageListResponse& response);
    void handleUploadFinished(const QString& tempId, const ApiDtos::UploadFileResponse& response,
                              const QString& messageType);
    void handleUploadFailed(const QString& tempId, const ApiDtos::ApiError& error);
    void fetchChatsWithRetry(int attempt);
    void fetchMessagesWithRetry(const QString& chatId, int attempt);
    void setErrorMessage(const QString& message);
    void setOffline(bool offline);
    void updateSelectedChatTitle();
    void handleNewMessage(const Message& message);
    QString buildAttachmentContent(const QString& messageType, const QString& fileName) const;
    QString resolveMessageType(const QString& filePath, const QString& fileName) const;

    ApiClient* api_ = nullptr;
    WsClient* ws_ = nullptr;
    UploadManager* uploads_ = nullptr;
    Session* session_ = nullptr;

    ChatListModel chatListModel_;
    MessageListModel messageListModel_;
    UploadListModel uploadListModel_;

    QString selectedChatId_;
    QString selectedChatTitle_;
    bool selectedChatOnline_ = false;
    bool loadingChats_ = false;
    bool loadingMessages_ = false;
    bool offline_ = false;
    QString errorMessage_;
    QString typingIndicator_;

    QTimer typingClearTimer_;
    int chatFetchAttempt_ = 0;
    QHash<QString, int> messageFetchAttempts_;
    QHash<QString, QString> uploadTargets_;
};

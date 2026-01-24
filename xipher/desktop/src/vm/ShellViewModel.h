#pragma once

#include <QObject>
#include <QString>

#include "store/ChatStore.h"

class ShellViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(ChatListModel* chatListModel READ chatListModel CONSTANT)
    Q_PROPERTY(MessageListModel* messageListModel READ messageListModel CONSTANT)
    Q_PROPERTY(UploadListModel* uploadListModel READ uploadListModel CONSTANT)
    Q_PROPERTY(QString selectedChatId READ selectedChatId NOTIFY selectedChatIdChanged)
    Q_PROPERTY(QString selectedChatTitle READ selectedChatTitle NOTIFY selectedChatTitleChanged)
    Q_PROPERTY(bool selectedChatOnline READ selectedChatOnline NOTIFY selectedChatOnlineChanged)
    Q_PROPERTY(QString typingIndicator READ typingIndicator NOTIFY typingIndicatorChanged)
    Q_PROPERTY(bool loadingChats READ loadingChats NOTIFY loadingChatsChanged)
    Q_PROPERTY(bool loadingMessages READ loadingMessages NOTIFY loadingMessagesChanged)
    Q_PROPERTY(bool offline READ offline NOTIFY offlineChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)

public:
    explicit ShellViewModel(ChatStore* store, QObject* parent = nullptr);

    ChatListModel* chatListModel();
    MessageListModel* messageListModel();
    UploadListModel* uploadListModel();

    QString selectedChatId() const;
    QString selectedChatTitle() const;
    bool selectedChatOnline() const;
    QString typingIndicator() const;
    bool loadingChats() const;
    bool loadingMessages() const;
    bool offline() const;
    QString errorMessage() const;
    QString searchQuery() const;
    void setSearchQuery(const QString& query);

    Q_INVOKABLE void selectChat(const QString& chatId);
    Q_INVOKABLE void sendMessage(const QString& text);
    Q_INVOKABLE void addAttachment(const QString& filePath);
    Q_INVOKABLE void cancelUpload(const QString& tempId);
    Q_INVOKABLE void retryMessage(const QString& messageId);
    Q_INVOKABLE void deleteMessage(const QString& messageId);
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void refreshChats();
    Q_INVOKABLE void sendTyping(bool isTyping);
    Q_INVOKABLE void findUser(const QString& username);
    Q_INVOKABLE void createGroup(const QString& name, const QStringList& memberIds);

signals:
    void selectedChatIdChanged();
    void selectedChatTitleChanged();
    void selectedChatOnlineChanged();
    void typingIndicatorChanged();
    void loadingChatsChanged();
    void loadingMessagesChanged();
    void offlineChanged();
    void errorMessageChanged();
    void searchQueryChanged();
    void toastRequested(const QString& type, const QString& message);
    void userFound(bool success, const QString& userId, const QString& username, 
                   const QString& avatarUrl, bool isPremium, const QString& message);
    void groupCreated(bool success, const QString& groupId, const QString& message);

private:
    ChatStore* store_ = nullptr;
    QString searchQuery_;
};

#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/Chat.h"

class ChatListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DisplayNameRole,
        AvatarRole,
        AvatarUrlRole,
        LastMessageRole,
        TimeRole,
        UnreadRole,
        OnlineRole,
        PinnedRole,
        MutedRole,
        LastActivityRole,
        IsSavedMessagesRole,
        IsBotRole,
        IsPremiumRole
    };

    explicit ChatListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setChats(const QVector<Chat>& chats);
    void setFilterText(const QString& filter);
    void updateChat(const Chat& chat);
    void incrementUnread(const QString& chatId);
    void clearUnread(const QString& chatId);
    Chat chatById(const QString& chatId) const;

private:
    void rebuildFilter();
    int indexInAll(const QString& chatId) const;
    int indexInFiltered(const QString& chatId) const;

    QVector<Chat> chats_;
    QVector<Chat> filtered_;
    QString filterText_;
};

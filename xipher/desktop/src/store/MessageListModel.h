#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "models/Message.h"

class MessageListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TempIdRole,
        SenderIdRole,
        ReceiverIdRole,
        ContentRole,
        MessageTypeRole,
        FilePathRole,
        FileNameRole,
        FileSizeRole,
        MimeTypeRole,
        LocalFilePathRole,
        TransferStateRole,
        TransferProgressRole,
        ReplyToMessageIdRole,
        TimeRole,
        CreatedAtRole,
        StatusRole,
        SentRole,
        IsReadRole,
        IsDeliveredRole,
        ChatIdRole,
        ChatTypeRole
    };

    explicit MessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QVector<Message>& messages);
    void appendMessage(const Message& message);
    void updateMessageTransfer(const QString& tempId, const QString& state, int progress);
    void updateMessageStatus(const QString& messageId, const QString& status, bool isRead, bool isDelivered);
    void removeMessage(const QString& messageId);
    void reconcileTempMessage(const QString& tempId, const Message& serverMessage);
    Message messageById(const QString& messageId) const;
    Message messageByTempId(const QString& tempId) const;

private:
    int indexOfMessage(const QString& messageId) const;
    int indexOfTemp(const QString& tempId) const;

    QVector<Message> messages_;
};

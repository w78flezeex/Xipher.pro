#include "store/MessageListModel.h"

MessageListModel::MessageListModel(QObject* parent) : QAbstractListModel(parent) {}

int MessageListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return messages_.size();
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= messages_.size()) {
        return QVariant();
    }
    const Message& msg = messages_.at(index.row());
    switch (role) {
        case IdRole:
            return msg.id;
        case TempIdRole:
            return msg.tempId;
        case SenderIdRole:
            return msg.senderId;
        case ReceiverIdRole:
            return msg.receiverId;
        case ContentRole:
            return msg.content;
        case MessageTypeRole:
            return msg.messageType;
        case FilePathRole:
            return msg.filePath;
        case FileNameRole:
            return msg.fileName;
        case FileSizeRole:
            return msg.fileSize;
        case MimeTypeRole:
            return msg.mimeType;
        case LocalFilePathRole:
            return msg.localFilePath;
        case TransferStateRole:
            return msg.transferState;
        case TransferProgressRole:
            return msg.transferProgress;
        case ReplyToMessageIdRole:
            return msg.replyToMessageId;
        case TimeRole:
            return msg.time;
        case CreatedAtRole:
            return msg.createdAt;
        case StatusRole:
            return msg.status;
        case SentRole:
            return msg.sent;
        case IsReadRole:
            return msg.isRead;
        case IsDeliveredRole:
            return msg.isDelivered;
        case ChatIdRole:
            return msg.chatId;
        case ChatTypeRole:
            return msg.chatType;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {TempIdRole, "tempId"},
        {SenderIdRole, "senderId"},
        {ReceiverIdRole, "receiverId"},
        {ContentRole, "content"},
        {MessageTypeRole, "messageType"},
        {FilePathRole, "filePath"},
        {FileNameRole, "fileName"},
        {FileSizeRole, "fileSize"},
        {MimeTypeRole, "mimeType"},
        {LocalFilePathRole, "localFilePath"},
        {TransferStateRole, "transferState"},
        {TransferProgressRole, "transferProgress"},
        {ReplyToMessageIdRole, "replyToMessageId"},
        {TimeRole, "time"},
        {CreatedAtRole, "createdAt"},
        {StatusRole, "status"},
        {SentRole, "sent"},
        {IsReadRole, "isRead"},
        {IsDeliveredRole, "isDelivered"},
        {ChatIdRole, "chatId"},
        {ChatTypeRole, "chatType"}
    };
}

void MessageListModel::setMessages(const QVector<Message>& messages) {
    beginResetModel();
    messages_ = messages;
    endResetModel();
}

void MessageListModel::appendMessage(const Message& message) {
    beginInsertRows(QModelIndex(), messages_.size(), messages_.size());
    messages_.push_back(message);
    endInsertRows();
}

void MessageListModel::updateMessageTransfer(const QString& tempId, const QString& state, int progress) {
    const int idx = indexOfTemp(tempId);
    if (idx == -1) {
        return;
    }
    Message& msg = messages_[idx];
    msg.transferState = state;
    msg.transferProgress = progress;
    const QModelIndex modelIndex = index(idx);
    emit dataChanged(modelIndex, modelIndex, {TransferStateRole, TransferProgressRole});
}

void MessageListModel::updateMessageStatus(const QString& messageId, const QString& status, bool isRead,
                                           bool isDelivered) {
    int idx = indexOfMessage(messageId);
    if (idx == -1) {
        idx = indexOfTemp(messageId);
        if (idx == -1) {
            return;
        }
    }
    Message& msg = messages_[idx];
    msg.status = status;
    msg.isRead = isRead;
    msg.isDelivered = isDelivered;
    const QModelIndex modelIndex = index(idx);
    emit dataChanged(modelIndex, modelIndex, {StatusRole, IsReadRole, IsDeliveredRole});
}

void MessageListModel::removeMessage(const QString& messageId) {
    int idx = indexOfMessage(messageId);
    if (idx == -1) {
        idx = indexOfTemp(messageId);
    }
    if (idx == -1) {
        return;
    }
    beginRemoveRows(QModelIndex(), idx, idx);
    messages_.removeAt(idx);
    endRemoveRows();
}

void MessageListModel::reconcileTempMessage(const QString& tempId, const Message& serverMessage) {
    const int idx = indexOfTemp(tempId);
    if (idx == -1) {
        return;
    }
    Message& msg = messages_[idx];
    msg.id = serverMessage.id;
    msg.createdAt = serverMessage.createdAt;
    msg.time = serverMessage.time;
    msg.status = serverMessage.status;
    msg.isRead = serverMessage.isRead;
    msg.isDelivered = serverMessage.isDelivered;
    msg.tempId = serverMessage.tempId.isEmpty() ? msg.tempId : serverMessage.tempId;
    msg.content = serverMessage.content.isEmpty() ? msg.content : serverMessage.content;
    msg.messageType = serverMessage.messageType.isEmpty() ? msg.messageType : serverMessage.messageType;
    msg.filePath = serverMessage.filePath;
    msg.fileName = serverMessage.fileName;
    msg.fileSize = serverMessage.fileSize;
    msg.mimeType = serverMessage.mimeType;
    msg.replyToMessageId = serverMessage.replyToMessageId;
    msg.transferState.clear();
    msg.transferProgress = 100;

    const QModelIndex modelIndex = index(idx);
    emit dataChanged(modelIndex, modelIndex);
}

Message MessageListModel::messageById(const QString& messageId) const {
    for (const auto& msg : messages_) {
        if (msg.id == messageId) {
            return msg;
        }
    }
    return Message();
}

Message MessageListModel::messageByTempId(const QString& tempId) const {
    for (const auto& msg : messages_) {
        if (msg.tempId == tempId) {
            return msg;
        }
    }
    return Message();
}

int MessageListModel::indexOfMessage(const QString& messageId) const {
    for (int i = 0; i < messages_.size(); ++i) {
        if (messages_[i].id == messageId) {
            return i;
        }
    }
    return -1;
}

int MessageListModel::indexOfTemp(const QString& tempId) const {
    for (int i = 0; i < messages_.size(); ++i) {
        if (messages_[i].tempId == tempId) {
            return i;
        }
    }
    return -1;
}

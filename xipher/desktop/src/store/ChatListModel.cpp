#include "store/ChatListModel.h"

ChatListModel::ChatListModel(QObject* parent) : QAbstractListModel(parent) {}

int ChatListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return filtered_.size();
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= filtered_.size()) {
        return QVariant();
    }

    const Chat& chat = filtered_.at(index.row());
    switch (role) {
        case IdRole:
            return chat.id;
        case NameRole:
            return chat.name;
        case DisplayNameRole:
            return chat.displayName;
        case AvatarRole:
            return chat.avatar;
        case AvatarUrlRole:
            return chat.avatarUrl;
        case LastMessageRole:
            return chat.lastMessage;
        case TimeRole:
            return chat.time;
        case UnreadRole:
            return chat.unread;
        case OnlineRole:
            return chat.online;
        case PinnedRole:
            return chat.pinned;
        case MutedRole:
            return chat.muted;
        case LastActivityRole:
            return chat.lastActivity;
        case IsSavedMessagesRole:
            return chat.isSavedMessages;
        case IsBotRole:
            return chat.isBot;
        case IsPremiumRole:
            return chat.isPremium;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> ChatListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {DisplayNameRole, "displayName"},
        {AvatarRole, "avatar"},
        {AvatarUrlRole, "avatarUrl"},
        {LastMessageRole, "lastMessage"},
        {TimeRole, "time"},
        {UnreadRole, "unread"},
        {OnlineRole, "online"},
        {PinnedRole, "pinned"},
        {MutedRole, "muted"},
        {LastActivityRole, "lastActivity"},
        {IsSavedMessagesRole, "isSavedMessages"},
        {IsBotRole, "isBot"},
        {IsPremiumRole, "isPremium"}
    };
}

void ChatListModel::setChats(const QVector<Chat>& chats) {
    beginResetModel();
    chats_ = chats;
    rebuildFilter();
    endResetModel();
}

void ChatListModel::setFilterText(const QString& filter) {
    if (filterText_ == filter) {
        return;
    }
    filterText_ = filter;
    beginResetModel();
    rebuildFilter();
    endResetModel();
}

void ChatListModel::updateChat(const Chat& chat) {
    const int idx = indexInAll(chat.id);
    if (idx == -1) {
        beginResetModel();
        chats_.push_back(chat);
        rebuildFilter();
        endResetModel();
        return;
    }

    chats_[idx] = chat;
    rebuildFilter();
    const int filteredIndex = indexInFiltered(chat.id);
    if (filteredIndex != -1) {
        const QModelIndex modelIndex = index(filteredIndex);
        emit dataChanged(modelIndex, modelIndex);
    }
}

void ChatListModel::incrementUnread(const QString& chatId) {
    const int idx = indexInAll(chatId);
    if (idx == -1) {
        return;
    }
    chats_[idx].unread += 1;
    rebuildFilter();
    const int filteredIndex = indexInFiltered(chatId);
    if (filteredIndex != -1) {
        const QModelIndex modelIndex = index(filteredIndex);
        emit dataChanged(modelIndex, modelIndex, {UnreadRole});
    }
}

void ChatListModel::clearUnread(const QString& chatId) {
    const int idx = indexInAll(chatId);
    if (idx == -1) {
        return;
    }
    chats_[idx].unread = 0;
    rebuildFilter();
    const int filteredIndex = indexInFiltered(chatId);
    if (filteredIndex != -1) {
        const QModelIndex modelIndex = index(filteredIndex);
        emit dataChanged(modelIndex, modelIndex, {UnreadRole});
    }
}

Chat ChatListModel::chatById(const QString& chatId) const {
    for (const auto& chat : chats_) {
        if (chat.id == chatId) {
            return chat;
        }
    }
    return Chat();
}

void ChatListModel::rebuildFilter() {
    if (filterText_.trimmed().isEmpty()) {
        filtered_ = chats_;
        return;
    }
    filtered_.clear();
    const QString needle = filterText_.trimmed().toLower();
    for (const auto& chat : chats_) {
        const QString hay = (chat.displayName + " " + chat.name).toLower();
        if (hay.contains(needle)) {
            filtered_.push_back(chat);
        }
    }
}

int ChatListModel::indexInAll(const QString& chatId) const {
    for (int i = 0; i < chats_.size(); ++i) {
        if (chats_[i].id == chatId) {
            return i;
        }
    }
    return -1;
}

int ChatListModel::indexInFiltered(const QString& chatId) const {
    for (int i = 0; i < filtered_.size(); ++i) {
        if (filtered_[i].id == chatId) {
            return i;
        }
    }
    return -1;
}

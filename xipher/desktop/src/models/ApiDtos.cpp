#include "models/ApiDtos.h"

#include <QJsonArray>

namespace ApiDtos {

bool toBool(const QJsonValue& value, bool defaultValue) {
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isString()) {
        const QString lowered = value.toString().trimmed().toLower();
        if (lowered == "true" || lowered == "1" || lowered == "yes") {
            return true;
        }
        if (lowered == "false" || lowered == "0" || lowered == "no") {
            return false;
        }
    }
    return defaultValue;
}

QString toString(const QJsonValue& value, const QString& defaultValue) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? "true" : "false";
    }
    return defaultValue;
}

qint64 toInt64(const QJsonValue& value, qint64 defaultValue) {
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : defaultValue;
    }
    return defaultValue;
}

static bool parseSuccess(const QJsonObject& obj) {
    if (obj.contains("success")) {
        return toBool(obj.value("success"));
    }
    return false;
}

LoginResponse parseLoginResponse(const QJsonObject& obj, int httpStatus) {
    LoginResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    const QJsonObject data = obj.value("data").toObject();
    response.userId = toString(data.value("user_id"));
    response.username = toString(data.value("username"));
    response.token = toString(data.value("token"));
    response.isPremium = toBool(data.value("is_premium"));
    response.premiumPlan = toString(data.value("premium_plan"));
    response.premiumExpiresAt = toString(data.value("premium_expires_at"));
    return response;
}

SessionResponse parseSessionResponse(const QJsonObject& obj, int httpStatus) {
    SessionResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    const QJsonObject data = obj.value("data").toObject();
    response.userId = toString(data.value("user_id"));
    response.username = toString(data.value("username"));
    response.isPremium = toBool(data.value("is_premium"));
    response.premiumPlan = toString(data.value("premium_plan"));
    response.premiumExpiresAt = toString(data.value("premium_expires_at"));
    return response;
}

ChatListResponse parseChatListResponse(const QJsonObject& obj, int httpStatus) {
    ChatListResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    const QJsonArray chats = obj.value("chats").toArray();
    response.chats.reserve(chats.size());
    for (const auto& entry : chats) {
        const QJsonObject chatObj = entry.toObject();
        Chat chat;
        chat.id = toString(chatObj.value("id"));
        chat.name = toString(chatObj.value("name"));
        chat.displayName = toString(chatObj.value("display_name"), chat.name);
        chat.avatar = toString(chatObj.value("avatar"));
        chat.avatarUrl = toString(chatObj.value("avatar_url"));
        chat.lastMessage = toString(chatObj.value("lastMessage"));
        chat.time = toString(chatObj.value("time"));
        chat.unread = static_cast<int>(toInt64(chatObj.value("unread")));
        chat.online = toBool(chatObj.value("online"));
        chat.pinned = toBool(chatObj.value("pinned"));
        chat.muted = toBool(chatObj.value("muted"));
        chat.lastActivity = toString(chatObj.value("last_activity"));
        chat.isSavedMessages = toBool(chatObj.value("is_saved_messages"));
        chat.isBot = toBool(chatObj.value("is_bot"));
        chat.isPremium = toBool(chatObj.value("is_premium"));
        response.chats.push_back(chat);
    }

    return response;
}

MessageListResponse parseMessageListResponse(const QJsonObject& obj, int httpStatus) {
    MessageListResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    const QJsonArray messages = obj.value("messages").toArray();
    response.messages.reserve(messages.size());
    for (const auto& entry : messages) {
        const QJsonObject msgObj = entry.toObject();
        Message msg;
        msg.id = toString(msgObj.value("id"));
        msg.senderId = toString(msgObj.value("sender_id"));
        msg.sent = toBool(msgObj.value("sent"));
        msg.status = toString(msgObj.value("status"));
        msg.isRead = toBool(msgObj.value("is_read"));
        msg.isDelivered = toBool(msgObj.value("is_delivered"));
        msg.content = toString(msgObj.value("content"));
        msg.messageType = toString(msgObj.value("message_type"), "text");
        msg.filePath = toString(msgObj.value("file_path"));
        msg.fileName = toString(msgObj.value("file_name"));
        msg.fileSize = toInt64(msgObj.value("file_size"));
        msg.mimeType = toString(msgObj.value("mime_type"));
        msg.replyToMessageId = toString(msgObj.value("reply_to_message_id"));
        msg.time = toString(msgObj.value("time"));
        msg.isRead = toBool(msgObj.value("is_read"));
        msg.isDelivered = toBool(msgObj.value("is_delivered"));
        response.messages.push_back(msg);
    }

    return response;
}

SendMessageResponse parseSendMessageResponse(const QJsonObject& obj, int httpStatus) {
    SendMessageResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    Message msg;
    msg.id = toString(obj.value("message_id"));
    msg.tempId = toString(obj.value("temp_id"));
    msg.createdAt = toString(obj.value("created_at"));
    msg.time = toString(obj.value("time"));
    msg.status = toString(obj.value("status"));
    msg.isRead = toBool(obj.value("is_read"));
    msg.isDelivered = toBool(obj.value("is_delivered"));
    msg.content = toString(obj.value("content"));
    msg.messageType = toString(obj.value("message_type"), "text");
    msg.filePath = toString(obj.value("file_path"));
    msg.fileName = toString(obj.value("file_name"));
    msg.fileSize = toInt64(obj.value("file_size"));
    msg.mimeType = toString(obj.value("mime_type"));
    msg.replyToMessageId = toString(obj.value("reply_to_message_id"));
    response.messageData = msg;

    return response;
}

Message parseMessageFromWs(const QJsonObject& obj, const QString& currentUserId) {
    QJsonObject source = obj;
    if (obj.contains("message") && obj.value("message").isObject()) {
        source = obj.value("message").toObject();
    }

    Message msg;
    msg.id = toString(source.value("id"));
    if (msg.id.isEmpty()) {
        msg.id = toString(source.value("message_id"));
    }
    msg.tempId = toString(source.value("temp_id"));
    msg.senderId = toString(source.value("sender_id"));
    msg.receiverId = toString(source.value("receiver_id"));
    msg.content = toString(source.value("content"));
    msg.messageType = toString(source.value("message_type"), "text");
    msg.filePath = toString(source.value("file_path"));
    msg.fileName = toString(source.value("file_name"));
    msg.fileSize = toInt64(source.value("file_size"));
    msg.mimeType = toString(source.value("mime_type"));
    msg.replyToMessageId = toString(source.value("reply_to_message_id"));
    msg.createdAt = toString(source.value("created_at"));
    msg.time = toString(source.value("time"));
    msg.status = toString(source.value("status"));
    msg.isRead = toBool(source.value("is_read"));
    msg.isDelivered = toBool(source.value("is_delivered"));
    msg.chatId = toString(source.value("chat_id"));
    msg.chatType = toString(source.value("chat_type"));

    if (!currentUserId.isEmpty() && !msg.senderId.isEmpty()) {
        msg.sent = (currentUserId == msg.senderId);
    }

    return msg;
}

UploadFileResponse parseUploadFileResponse(const QJsonObject& obj, int httpStatus) {
    UploadFileResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    response.filePath = toString(obj.value("file_path"));
    response.fileName = toString(obj.value("file_name"));
    response.fileSize = toInt64(obj.value("file_size"));
    return response;
}

UserProfileResponse parseUserProfileResponse(const QJsonObject& obj, int httpStatus) {
    UserProfileResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    const QJsonObject user = obj.value("user").toObject();
    response.firstName = toString(user.value("first_name"));
    response.lastName = toString(user.value("last_name"));
    response.bio = toString(user.value("bio"));
    response.avatarUrl = toString(user.value("avatar_url"));
    response.isPremium = toBool(user.value("is_premium"));
    response.premiumPlan = toString(user.value("premium_plan"));
    response.premiumExpiresAt = toString(user.value("premium_expires_at"));
    return response;
}

FindUserResponse parseFindUserResponse(const QJsonObject& obj, int httpStatus) {
    FindUserResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    response.userId = toString(obj.value("user_id"));
    response.username = toString(obj.value("username"));
    response.avatarUrl = toString(obj.value("avatar_url"));
    response.isPremium = toBool(obj.value("is_premium"));
    return response;
}

CreateGroupResponse parseCreateGroupResponse(const QJsonObject& obj, int httpStatus) {
    CreateGroupResponse response;
    response.success = parseSuccess(obj);
    response.message = toString(obj.value("message"));

    if (!response.success) {
        response.error.httpStatus = httpStatus;
        response.error.userMessage = response.message;
        return response;
    }

    response.groupId = toString(obj.value("group_id"));
    response.groupName = toString(obj.value("group_name"));
    return response;
}

}  // namespace ApiDtos

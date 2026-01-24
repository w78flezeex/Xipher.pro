#pragma once

#include <QJsonObject>
#include <QVector>
#include <QString>

#include "models/Chat.h"
#include "models/Message.h"

namespace ApiDtos {

struct ApiError {
    int httpStatus = 0;
    QString serverCode;
    QString userMessage;
    QString debugMessage;
    bool isNetworkError = false;
    bool isTransient = false;
};

struct LoginResponse {
    bool success = false;
    QString message;
    QString userId;
    QString username;
    QString token;
    bool isPremium = false;
    QString premiumPlan;
    QString premiumExpiresAt;
    ApiError error;
};

struct SessionResponse {
    bool success = false;
    QString message;
    QString userId;
    QString username;
    bool isPremium = false;
    QString premiumPlan;
    QString premiumExpiresAt;
    ApiError error;
};

struct ChatListResponse {
    bool success = false;
    QString message;
    QVector<Chat> chats;
    ApiError error;
};

struct MessageListResponse {
    bool success = false;
    QString message;
    QVector<Message> messages;
    ApiError error;
};

struct SendMessageResponse {
    bool success = false;
    QString message;
    Message messageData;
    ApiError error;
};

struct UploadFileResponse {
    bool success = false;
    QString message;
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    ApiError error;
};

struct UserProfileResponse {
    bool success = false;
    QString message;
    QString firstName;
    QString lastName;
    QString bio;
    QString avatarUrl;
    bool isPremium = false;
    QString premiumPlan;
    QString premiumExpiresAt;
    ApiError error;
};

struct FindUserResponse {
    bool success = false;
    QString message;
    QString userId;
    QString username;
    QString avatarUrl;
    bool isPremium = false;
    ApiError error;
};

struct CreateGroupResponse {
    bool success = false;
    QString message;
    QString groupId;
    QString groupName;
    ApiError error;
};

bool toBool(const QJsonValue& value, bool defaultValue = false);
QString toString(const QJsonValue& value, const QString& defaultValue = QString());
qint64 toInt64(const QJsonValue& value, qint64 defaultValue = 0);

LoginResponse parseLoginResponse(const QJsonObject& obj, int httpStatus);
SessionResponse parseSessionResponse(const QJsonObject& obj, int httpStatus);
ChatListResponse parseChatListResponse(const QJsonObject& obj, int httpStatus);
MessageListResponse parseMessageListResponse(const QJsonObject& obj, int httpStatus);
SendMessageResponse parseSendMessageResponse(const QJsonObject& obj, int httpStatus);
Message parseMessageFromWs(const QJsonObject& obj, const QString& currentUserId);
UploadFileResponse parseUploadFileResponse(const QJsonObject& obj, int httpStatus);
UserProfileResponse parseUserProfileResponse(const QJsonObject& obj, int httpStatus);
FindUserResponse parseFindUserResponse(const QJsonObject& obj, int httpStatus);
CreateGroupResponse parseCreateGroupResponse(const QJsonObject& obj, int httpStatus);

}  // namespace ApiDtos

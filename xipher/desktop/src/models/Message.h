#pragma once

#include <QString>

struct Message {
    QString id;
    QString tempId;
    QString senderId;
    QString receiverId;
    QString content;
    QString messageType;
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    QString mimeType;
    QString localFilePath;
    QString transferState;
    int transferProgress = 0;
    QString replyToMessageId;
    QString time;
    QString createdAt;
    QString status;
    bool sent = false;
    bool isRead = false;
    bool isDelivered = false;
    QString chatId;
    QString chatType;
};

Q_DECLARE_METATYPE(Message)

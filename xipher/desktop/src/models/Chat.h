#pragma once

#include <QString>

struct Chat {
    QString id;
    QString name;
    QString displayName;
    QString avatar;
    QString avatarUrl;
    QString lastMessage;
    QString time;
    int unread = 0;
    bool online = false;
    bool pinned = false;
    bool muted = false;
    QString lastActivity;
    bool isSavedMessages = false;
    bool isBot = false;
    bool isPremium = false;
};

Q_DECLARE_METATYPE(Chat)

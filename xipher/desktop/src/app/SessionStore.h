#pragma once

#include "app/ISecureStorage.h"

#include <optional>

struct SessionInfo {
    QString userId;
    QString username;
    QString token;
    QString baseUrl;
};

class SessionStore {
public:
    explicit SessionStore(ISecureStorage* storage);

    bool saveSession(const SessionInfo& info);
    std::optional<SessionInfo> loadSession();
    bool clearSession();

    bool saveCookies(const QByteArray& data);
    std::optional<QByteArray> loadCookies();
    bool clearCookies();

    static bool migrateLegacy(ISecureStorage* legacy, ISecureStorage* target);

private:
    ISecureStorage* storage_ = nullptr;
};

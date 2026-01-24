#pragma once

#include <QObject>

#include "app/SessionStore.h"
#include "app/SecureStorage.h"
#include "models/ApiDtos.h"

class ApiClient;
class CookieJar;
class Session;
class SettingsStore;

class SessionManager : public QObject {
    Q_OBJECT

public:
    SessionManager(Session* session, SettingsStore* settings, ApiClient* api, CookieJar* cookieJar,
                   QObject* parent = nullptr);

    void restore();
    void persistFromLogin(const ApiDtos::LoginResponse& response);
    void clear();

signals:
    void restoreCompleted(bool success);
    void sessionReady();

private:
    void handleSessionValidated(const ApiDtos::SessionResponse& response);
    void migrateLegacyIfNeeded();
    void persistSession(const QString& userId, const QString& username, const QString& token);
    void persistCookies();
    QString baseUrl() const;
    QString resolveToken(const QString& candidate) const;

    Session* session_ = nullptr;
    SettingsStore* settings_ = nullptr;
    ApiClient* api_ = nullptr;
    CookieJar* cookieJar_ = nullptr;
    SecureStorage storage_;
    SessionStore store_;
    bool restoreInFlight_ = false;
    QString legacyPath_;
    bool legacyChecked_ = false;
};

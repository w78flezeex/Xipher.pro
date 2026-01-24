#include "app/SessionManager.h"

#include <QDir>
#include <QFile>
#include <QLatin1String>
#include <QStandardPaths>
#include <QUrl>

#include "app/FileSecureStorage.h"
#include "app/Session.h"
#include "app/SettingsStore.h"
#include "logging/Log.h"
#include "net/ApiClient.h"
#include "net/CookieJar.h"

namespace {
QString legacyStoragePath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("secure_store.json");
}

QString secureStoragePath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("secure_store.dpapi.json");
}

QString appId() {
    return QStringLiteral("XipherDesktop");
}

const char kTokenPlaceholder[] = "cookie";
}  // namespace

SessionManager::SessionManager(Session* session, SettingsStore* settings, ApiClient* api, CookieJar* cookieJar,
                               QObject* parent)
    : QObject(parent),
      session_(session),
      settings_(settings),
      api_(api),
      cookieJar_(cookieJar),
      storage_(appId(), secureStoragePath()),
      store_(&storage_),
      legacyPath_(legacyStoragePath()) {
    if (api_) {
        connect(api_, &ApiClient::sessionValidated, this, &SessionManager::handleSessionValidated);
    }
    if (settings_) {
        connect(settings_, &SettingsStore::baseUrlChanged, this, [this]() { clear(); });
    }
}

void SessionManager::restore() {
    if (restoreInFlight_) {
        return;
    }
    if (!session_ || !api_ || !cookieJar_) {
        return;
    }
    migrateLegacyIfNeeded();
    const std::optional<SessionInfo> info = store_.loadSession();
    const std::optional<QByteArray> cookieBlob = store_.loadCookies();
    if (!info || !cookieBlob || cookieBlob->isEmpty()) {
        return;
    }
    if (!info->baseUrl.isEmpty() && info->baseUrl != baseUrl()) {
        clear();
        return;
    }

    cookieJar_->deserialize(*cookieBlob);
    const QString token = resolveToken(info->token);
    if (token.isEmpty()) {
        clear();
        return;
    }
    restoreInFlight_ = true;
    session_->setRestoring(true);
    session_->setSession(info->userId, info->username, token);
    api_->validateToken();
}

void SessionManager::persistFromLogin(const ApiDtos::LoginResponse& response) {
    if (!response.success || !session_ || !cookieJar_) {
        return;
    }
    if (!response.token.isEmpty()) {
        cookieJar_->setSessionCookie(baseUrl(), response.token);
    }
    const QString token = resolveToken(response.token);
    if (token.isEmpty()) {
        clear();
        return;
    }
    session_->setSession(response.userId, response.username, token);
    persistSession(response.userId, response.username, token);
    persistCookies();
    emit sessionReady();
}

void SessionManager::clear() {
    restoreInFlight_ = false;
    store_.clearSession();
    store_.clearCookies();
    if (cookieJar_) {
        cookieJar_->clearSessionCookie();
    }
    if (!legacyPath_.isEmpty()) {
        QFile::remove(legacyPath_);
    }
    if (session_) {
        session_->clear();
        session_->setRestoring(false);
    }
}

void SessionManager::handleSessionValidated(const ApiDtos::SessionResponse& response) {
    if (!restoreInFlight_) {
        return;
    }
    restoreInFlight_ = false;
    if (session_) {
        session_->setRestoring(false);
    }
    if (!response.success) {
        clear();
        emit restoreCompleted(false);
        return;
    }
    const QString token = resolveToken(session_ ? session_->token() : QString());
    if (session_) {
        session_->setSession(response.userId, response.username, token);
    }
    persistSession(response.userId, response.username, token);
    persistCookies();
    emit restoreCompleted(true);
    emit sessionReady();
}

void SessionManager::migrateLegacyIfNeeded() {
    if (legacyChecked_) {
        return;
    }
    legacyChecked_ = true;
    if (legacyPath_.isEmpty() || !QFile::exists(legacyPath_)) {
        return;
    }
    if (store_.loadSession() || store_.loadCookies()) {
        QFile::remove(legacyPath_);
        return;
    }
    FileSecureStorage legacy(legacyPath_);
    const bool migrated = SessionStore::migrateLegacy(&legacy, &storage_);
    if (migrated) {
        QFile::remove(legacyPath_);
    }
}

void SessionManager::persistSession(const QString& userId, const QString& username, const QString& token) {
    SessionInfo info;
    info.userId = userId;
    info.username = username;
    info.token = token;
    info.baseUrl = baseUrl();
    if (!store_.saveSession(info)) {
        qCWarning(logApp) << "Failed to persist session info";
    }
}

void SessionManager::persistCookies() {
    if (!cookieJar_) {
        return;
    }
    const QByteArray data = cookieJar_->serialize();
    if (data.isEmpty()) {
        return;
    }
    if (!store_.saveCookies(data)) {
        qCWarning(logApp) << "Failed to persist cookies";
    }
}

QString SessionManager::baseUrl() const {
    return settings_ ? settings_->baseUrl() : QString();
}

QString SessionManager::resolveToken(const QString& candidate) const {
    const QString trimmed = candidate.trimmed();
    if (!trimmed.isEmpty() && trimmed != QLatin1String(kTokenPlaceholder)) {
        return trimmed;
    }
    if (!cookieJar_) {
        return QString();
    }
    const QUrl url(baseUrl());
    const QString cookieToken = cookieJar_->sessionTokenForUrl(url);
    if (!cookieToken.isEmpty()) {
        return cookieToken;
    }
    return QString();
}

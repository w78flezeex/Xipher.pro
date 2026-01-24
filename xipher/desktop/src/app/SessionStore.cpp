#include "app/SessionStore.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {
const char kSessionInfoKey[] = "session_info";
const char kSessionCookiesKey[] = "session_cookies";
}  // namespace

SessionStore::SessionStore(ISecureStorage* storage) : storage_(storage) {}

bool SessionStore::saveSession(const SessionInfo& info) {
    if (!storage_) {
        return false;
    }
    QJsonObject obj;
    obj.insert("user_id", info.userId);
    obj.insert("username", info.username);
    obj.insert("token", info.token);
    obj.insert("base_url", info.baseUrl);
    const QJsonDocument doc(obj);
    return storage_->setSecret(kSessionInfoKey, doc.toJson(QJsonDocument::Compact));
}

std::optional<SessionInfo> SessionStore::loadSession() {
    if (!storage_) {
        return std::nullopt;
    }
    const std::optional<QByteArray> data = storage_->getSecret(kSessionInfoKey);
    if (!data || data->isEmpty()) {
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(*data);
    if (!doc.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = doc.object();
    SessionInfo info;
    info.userId = obj.value("user_id").toString();
    info.username = obj.value("username").toString();
    info.token = obj.value("token").toString();
    info.baseUrl = obj.value("base_url").toString();
    return info;
}

bool SessionStore::clearSession() {
    if (!storage_) {
        return false;
    }
    return storage_->deleteSecret(kSessionInfoKey);
}

bool SessionStore::saveCookies(const QByteArray& data) {
    if (!storage_) {
        return false;
    }
    return storage_->setSecret(kSessionCookiesKey, data);
}

std::optional<QByteArray> SessionStore::loadCookies() {
    if (!storage_) {
        return std::nullopt;
    }
    return storage_->getSecret(kSessionCookiesKey);
}

bool SessionStore::clearCookies() {
    if (!storage_) {
        return false;
    }
    return storage_->deleteSecret(kSessionCookiesKey);
}

bool SessionStore::migrateLegacy(ISecureStorage* legacy, ISecureStorage* target) {
    if (!legacy || !target) {
        return false;
    }
    bool migrated = false;

    const std::optional<QByteArray> session = legacy->getSecret(kSessionInfoKey);
    if (session && !session->isEmpty()) {
        if (target->setSecret(kSessionInfoKey, *session)) {
            legacy->deleteSecret(kSessionInfoKey);
            migrated = true;
        }
    }

    const std::optional<QByteArray> cookies = legacy->getSecret(kSessionCookiesKey);
    if (cookies && !cookies->isEmpty()) {
        if (target->setSecret(kSessionCookiesKey, *cookies)) {
            legacy->deleteSecret(kSessionCookiesKey);
            migrated = true;
        }
    }

    return migrated;
}

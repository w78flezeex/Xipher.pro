#include "app/AppConfig.h"

#include <QRegularExpression>
#include <QUrl>

AppConfig::AppConfig(QObject* parent) : QObject(parent) {}

QString AppConfig::baseUrl() const {
    return baseUrl_;
}

void AppConfig::setBaseUrl(const QString& baseUrl) {
    const QString normalized = normalizeBaseUrl(baseUrl);
    if (normalized == baseUrl_) {
        return;
    }
    baseUrl_ = normalized;
    emit baseUrlChanged();
    emit wsUrlChanged();
}

QString AppConfig::wsUrl() const {
    if (!wsUrlOverride_.isEmpty()) {
        return wsUrlOverride_;
    }
    return buildWsUrlFromBase(baseUrl_);
}

void AppConfig::setWsUrl(const QString& wsUrl) {
    if (wsUrlOverride_ == wsUrl) {
        return;
    }
    wsUrlOverride_ = wsUrl;
    emit wsUrlChanged();
}

int AppConfig::requestTimeoutMs() const {
    return 15000;
}

int AppConfig::wsReconnectBaseMs() const {
    return 1000;
}

qint64 AppConfig::uploadMaxBytes() const {
    return uploadMaxBytes_;
}

void AppConfig::setUploadMaxBytes(qint64 bytes) {
    if (bytes <= 0 || uploadMaxBytes_ == bytes) {
        return;
    }
    uploadMaxBytes_ = bytes;
    emit uploadMaxBytesChanged();
}

bool AppConfig::callsEnabled() const {
    return enableCalls_;
}

void AppConfig::setCallsEnabled(bool enabled) {
    setEnableCalls(enabled);
}

bool AppConfig::enableCalls() const {
    return enableCalls_;
}

void AppConfig::setEnableCalls(bool enabled) {
    if (enableCalls_ == enabled) {
        return;
    }
    enableCalls_ = enabled;
    emit enableCallsChanged();
    emit callsEnabledChanged();
}

bool AppConfig::enableBots() const {
    return enableBots_;
}

void AppConfig::setEnableBots(bool enabled) {
    if (enableBots_ == enabled) {
        return;
    }
    enableBots_ = enabled;
    emit enableBotsChanged();
}

bool AppConfig::enableAdmin() const {
    return enableAdmin_;
}

void AppConfig::setEnableAdmin(bool enabled) {
    if (enableAdmin_ == enabled) {
        return;
    }
    enableAdmin_ = enabled;
    emit enableAdminChanged();
}

bool AppConfig::enableMarketplace() const {
    return enableMarketplace_;
}

void AppConfig::setEnableMarketplace(bool enabled) {
    if (enableMarketplace_ == enabled) {
        return;
    }
    enableMarketplace_ = enabled;
    emit enableMarketplaceChanged();
}

QUrl AppConfig::buildUrl(const QString& path) const {
    QUrl url(baseUrl_);
    if (!url.isValid()) {
        return QUrl();
    }
    QString cleaned = path.trimmed();
    if (!cleaned.startsWith('/')) {
        cleaned.prepend('/');
    }
    url.setPath(cleaned);
    return url;
}

QString AppConfig::normalizeBaseUrl(const QString& baseUrl) const {
    QString trimmed = baseUrl.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    if (!trimmed.contains("://")) {
        const QUrl guess = QUrl::fromUserInput(trimmed);
        const QString host = guess.host().toLower();
        const bool loopback = host == "127.0.0.1" || host == "localhost" || host == "0.0.0.0" || host == "::1";
        trimmed.prepend(loopback ? "http://" : "https://");
    }
    if (trimmed.endsWith('/')) {
        trimmed.chop(1);
    }
    return trimmed;
}

QString AppConfig::buildWsUrlFromBase(const QString& baseUrl) const {
    if (baseUrl.isEmpty()) {
        return QString();
    }
    QUrl url(baseUrl);
    if (!url.isValid()) {
        return QString();
    }
    const QString scheme = url.scheme().toLower();
    if (scheme == "https") {
        url.setScheme("wss");
    } else {
        url.setScheme("ws");
    }
    url.setPath("/ws");
    return url.toString();
}

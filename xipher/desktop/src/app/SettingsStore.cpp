#include "app/SettingsStore.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QUrl>

namespace {
const char kOrgName[] = "Xipher";
const char kAppName[] = "XipherDesktop";
const char kBaseUrlKey[] = "network/baseUrl";
const char kThemeKey[] = "ui/theme";
const char kLangKey[] = "ui/lang";
const char kDesktopNotifKey[] = "notifications/desktop";
const char kSoundNotifKey[] = "notifications/sound";
const char kShowPreviewKey[] = "notifications/preview";
const char kDefaultBaseUrl[] = "https://xipher.pro";
}  // namespace

SettingsStore::SettingsStore(QObject* parent) : QObject(parent) {
    QCoreApplication::setOrganizationName(kOrgName);
    QCoreApplication::setApplicationName(kAppName);
    settings_ = new QSettings(this);

    baseUrl_ = settings_->value(kBaseUrlKey, QString()).toString();
    theme_ = settings_->value(kThemeKey, QString("dark")).toString();
    language_ = settings_->value(kLangKey, QString("en")).toString();
    desktopNotifications_ = settings_->value(kDesktopNotifKey, true).toBool();
    soundNotifications_ = settings_->value(kSoundNotifKey, true).toBool();
    showPreview_ = settings_->value(kShowPreviewKey, true).toBool();

    const QByteArray envBaseUrl = qgetenv("XIPHER_BASE_URL");
    const bool hasEnvBaseUrl = !envBaseUrl.isEmpty();
    if (!envBaseUrl.isEmpty()) {
        baseUrl_ = QString::fromUtf8(envBaseUrl);
    }
    if (baseUrl_.trimmed().isEmpty()) {
        baseUrl_ = QString::fromLatin1(kDefaultBaseUrl);
        settings_->setValue(kBaseUrlKey, baseUrl_);
        return;
    }
    if (hasEnvBaseUrl) {
        return;
    }
    const QUrl url = QUrl::fromUserInput(baseUrl_);
    const QString host = url.host().toLower();
    const bool isLoopback = host == "127.0.0.1" || host == "localhost" || host == "0.0.0.0" || host == "::1";
    if (isLoopback) {
        baseUrl_ = QString::fromLatin1(kDefaultBaseUrl);
        settings_->setValue(kBaseUrlKey, baseUrl_);
    }
}

QString SettingsStore::baseUrl() const {
    return baseUrl_;
}

void SettingsStore::setBaseUrl(const QString& baseUrl) {
    if (baseUrl_ == baseUrl) {
        return;
    }
    baseUrl_ = baseUrl.trimmed();
    settings_->setValue(kBaseUrlKey, baseUrl_);
    emit baseUrlChanged();
}

QString SettingsStore::theme() const {
    return theme_;
}

void SettingsStore::setTheme(const QString& theme) {
    if (theme_ == theme) {
        return;
    }
    theme_ = theme;
    settings_->setValue(kThemeKey, theme_);
    emit themeChanged();
}

QString SettingsStore::effectiveTheme() const {
    if (theme_ == "system") {
        return resolveSystemTheme();
    }
    return theme_;
}

QString SettingsStore::language() const {
    return language_;
}

void SettingsStore::setLanguage(const QString& language) {
    if (language_ == language) {
        return;
    }
    language_ = language;
    settings_->setValue(kLangKey, language_);
    emit languageChanged();
}

bool SettingsStore::desktopNotifications() const {
    return desktopNotifications_;
}

void SettingsStore::setDesktopNotifications(bool value) {
    if (desktopNotifications_ == value) return;
    desktopNotifications_ = value;
    settings_->setValue(kDesktopNotifKey, value);
    emit desktopNotificationsChanged();
}

bool SettingsStore::soundNotifications() const {
    return soundNotifications_;
}

void SettingsStore::setSoundNotifications(bool value) {
    if (soundNotifications_ == value) return;
    soundNotifications_ = value;
    settings_->setValue(kSoundNotifKey, value);
    emit soundNotificationsChanged();
}

bool SettingsStore::showPreview() const {
    return showPreview_;
}

void SettingsStore::setShowPreview(bool value) {
    if (showPreview_ == value) return;
    showPreview_ = value;
    settings_->setValue(kShowPreviewKey, value);
    emit showPreviewChanged();
}

QString SettingsStore::resolveSystemTheme() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Light ? "light" : "dark";
#else
    return "dark";
#endif
}

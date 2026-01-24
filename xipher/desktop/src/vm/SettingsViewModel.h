#pragma once

#include <QObject>
#include <QString>

#include "app/AppConfig.h"
#include "app/SettingsStore.h"
#include "app/Session.h"
#include "net/ApiClient.h"

class SettingsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    
    // User profile properties
    Q_PROPERTY(QString firstName READ firstName WRITE setFirstName NOTIFY firstNameChanged)
    Q_PROPERTY(QString lastName READ lastName WRITE setLastName NOTIFY lastNameChanged)
    Q_PROPERTY(QString bio READ bio WRITE setBio NOTIFY bioChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(bool isPremium READ isPremium NOTIFY isPremiumChanged)
    Q_PROPERTY(QString premiumPlan READ premiumPlan NOTIFY premiumPlanChanged)
    Q_PROPERTY(bool profileLoading READ profileLoading NOTIFY profileLoadingChanged)
    
    // Notification settings
    Q_PROPERTY(bool desktopNotifications READ desktopNotifications WRITE setDesktopNotifications NOTIFY desktopNotificationsChanged)
    Q_PROPERTY(bool soundNotifications READ soundNotifications WRITE setSoundNotifications NOTIFY soundNotificationsChanged)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)

public:
    explicit SettingsViewModel(SettingsStore* settings, AppConfig* config, ApiClient* api,
                               Session* session, QObject* parent = nullptr);

    QString baseUrl() const;
    void setBaseUrl(const QString& baseUrl);

    QString theme() const;
    void setTheme(const QString& theme);

    QString language() const;
    void setLanguage(const QString& language);

    QString statusMessage() const;
    
    // Profile getters/setters
    QString firstName() const;
    void setFirstName(const QString& value);
    QString lastName() const;
    void setLastName(const QString& value);
    QString bio() const;
    void setBio(const QString& value);
    QString avatarUrl() const;
    bool isPremium() const;
    QString premiumPlan() const;
    bool profileLoading() const;
    
    // Notification getters/setters
    bool desktopNotifications() const;
    void setDesktopNotifications(bool value);
    bool soundNotifications() const;
    void setSoundNotifications(bool value);
    bool showPreview() const;
    void setShowPreview(bool value);

    Q_INVOKABLE void revalidateSession();
    Q_INVOKABLE void loadProfile();
    Q_INVOKABLE void saveProfile();

signals:
    void baseUrlChanged();
    void themeChanged();
    void languageChanged();
    void statusMessageChanged();
    void firstNameChanged();
    void lastNameChanged();
    void bioChanged();
    void avatarUrlChanged();
    void isPremiumChanged();
    void premiumPlanChanged();
    void profileLoadingChanged();
    void desktopNotificationsChanged();
    void soundNotificationsChanged();
    void showPreviewChanged();
    void profileSaved(bool success, const QString& message);

private:
    void setStatusMessage(const QString& message);

    SettingsStore* settings_ = nullptr;
    AppConfig* config_ = nullptr;
    ApiClient* api_ = nullptr;
    Session* session_ = nullptr;
    QString statusMessage_;
    
    // Profile data
    QString firstName_;
    QString lastName_;
    QString bio_;
    QString avatarUrl_;
    bool isPremium_ = false;
    QString premiumPlan_;
    bool profileLoading_ = false;
    
    // Notification settings
    bool desktopNotifications_ = true;
    bool soundNotifications_ = true;
    bool showPreview_ = true;
};

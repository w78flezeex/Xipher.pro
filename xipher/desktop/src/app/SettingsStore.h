#pragma once

#include <QObject>
#include <QString>

class QSettings;

class SettingsStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool desktopNotifications READ desktopNotifications WRITE setDesktopNotifications NOTIFY desktopNotificationsChanged)
    Q_PROPERTY(bool soundNotifications READ soundNotifications WRITE setSoundNotifications NOTIFY soundNotificationsChanged)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)

public:
    explicit SettingsStore(QObject* parent = nullptr);

    QString baseUrl() const;
    void setBaseUrl(const QString& baseUrl);

    QString theme() const;
    void setTheme(const QString& theme);

    QString effectiveTheme() const;

    QString language() const;
    void setLanguage(const QString& language);
    
    bool desktopNotifications() const;
    void setDesktopNotifications(bool value);
    
    bool soundNotifications() const;
    void setSoundNotifications(bool value);
    
    bool showPreview() const;
    void setShowPreview(bool value);

signals:
    void baseUrlChanged();
    void themeChanged();
    void languageChanged();
    void desktopNotificationsChanged();
    void soundNotificationsChanged();
    void showPreviewChanged();

private:
    QString resolveSystemTheme() const;

    QSettings* settings_ = nullptr;
    QString baseUrl_;
    QString theme_;
    QString language_;
    bool desktopNotifications_ = true;
    bool soundNotifications_ = true;
    bool showPreview_ = true;
};

#pragma once

#include <QObject>
#include <QString>

#include "app/SessionManager.h"
#include "net/ApiClient.h"

class SettingsStore;

class AuthViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY errorKeyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit AuthViewModel(ApiClient* api, SessionManager* sessionManager, SettingsStore* settings,
                           QObject* parent = nullptr);

    QString username() const;
    void setUsername(const QString& username);

    QString password() const;
    void setPassword(const QString& password);

    bool busy() const;
    QString errorKey() const;
    QString error() const;

    Q_INVOKABLE void login();

signals:
    void usernameChanged();
    void passwordChanged();
    void busyChanged();
    void errorKeyChanged();
    void errorChanged();

private:
    void setErrorKey(const QString& key);
    QString localizedErrorKey(const ApiDtos::LoginResponse& response) const;

    ApiClient* api_ = nullptr;
    SessionManager* sessionManager_ = nullptr;
    SettingsStore* settings_ = nullptr;

    QString username_;
    QString password_;
    bool busy_ = false;
    QString errorKey_;
    QString error_;
};

#include "vm/AuthViewModel.h"

AuthViewModel::AuthViewModel(ApiClient* api, SessionManager* sessionManager, SettingsStore* settings,
                             QObject* parent)
    : QObject(parent), api_(api), sessionManager_(sessionManager), settings_(settings) {
    if (api_) {
        connect(api_, &ApiClient::loginFinished, this, [this](const ApiDtos::LoginResponse& response) {
            busy_ = false;
            emit busyChanged();

            password_.clear();
            emit passwordChanged();

            if (!response.success) {
                setErrorKey(localizedErrorKey(response));
                if (errorKey_.isEmpty()) {
                    error_ = response.error.userMessage.isEmpty() ? response.message : response.error.userMessage;
                    if (error_.isEmpty()) {
                        error_ = "Login failed.";
                    }
                } else {
                    error_.clear();
                }
                emit errorChanged();
                return;
            }

            error_.clear();
            setErrorKey(QString());
            emit errorChanged();

            if (sessionManager_) {
                sessionManager_->persistFromLogin(response);
            }
        });
    }
}

QString AuthViewModel::username() const {
    return username_;
}

void AuthViewModel::setUsername(const QString& username) {
    if (username_ == username) {
        return;
    }
    username_ = username;
    emit usernameChanged();
}

QString AuthViewModel::password() const {
    return password_;
}

void AuthViewModel::setPassword(const QString& password) {
    if (password_ == password) {
        return;
    }
    password_ = password;
    emit passwordChanged();
}

bool AuthViewModel::busy() const {
    return busy_;
}

QString AuthViewModel::errorKey() const {
    return errorKey_;
}

QString AuthViewModel::error() const {
    return error_;
}

void AuthViewModel::login() {
    if (!api_) {
        return;
    }
    if (username_.trimmed().isEmpty() || password_.isEmpty()) {
        setErrorKey("auth.error.missing_credentials");
        error_.clear();
        emit errorChanged();
        return;
    }
    error_.clear();
    setErrorKey(QString());
    emit errorChanged();
    busy_ = true;
    emit busyChanged();

    api_->login(username_.trimmed(), password_);
}

void AuthViewModel::setErrorKey(const QString& key) {
    if (errorKey_ == key) {
        return;
    }
    errorKey_ = key;
    emit errorKeyChanged();
}

QString AuthViewModel::localizedErrorKey(const ApiDtos::LoginResponse& response) const {
    const ApiDtos::ApiError& error = response.error;
    if (error.isNetworkError || error.userMessage == "Network error") {
        return "auth.error.network";
    }
    if (error.userMessage == "Invalid base URL") {
        return "auth.error.invalid_base_url";
    }
    if (error.userMessage == "Invalid JSON response") {
        return "auth.error.invalid_response";
    }
    if (response.message.isEmpty() && error.userMessage.isEmpty()) {
        return "auth.error.login_failed";
    }
    return QString();
}

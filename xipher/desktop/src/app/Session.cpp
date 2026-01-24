#include "app/Session.h"

Session::Session(QObject* parent) : QObject(parent) {}

bool Session::isAuthed() const {
    return isAuthed_;
}

bool Session::isRestoring() const {
    return isRestoring_;
}

QString Session::userId() const {
    return userId_;
}

QString Session::username() const {
    return username_;
}

QString Session::token() const {
    return token_;
}

void Session::setSession(const QString& userId, const QString& username, const QString& token) {
    const bool wasAuthed = isAuthed_;
    const QString oldUserId = userId_;
    const QString oldUsername = username_;

    userId_ = userId;
    username_ = username;
    token_ = token;
    isAuthed_ = !userId_.isEmpty() && !token_.isEmpty();

    if (wasAuthed != isAuthed_) {
        emit isAuthedChanged();
    }
    if (oldUserId != userId_) {
        emit userIdChanged();
    }
    if (oldUsername != username_) {
        emit usernameChanged();
    }
}

void Session::clear() {
    const bool wasAuthed = isAuthed_;
    isAuthed_ = false;
    userId_.clear();
    username_.clear();
    token_.clear();

    if (wasAuthed) {
        emit isAuthedChanged();
    }
    emit userIdChanged();
    emit usernameChanged();
}

void Session::setRestoring(bool restoring) {
    if (isRestoring_ == restoring) {
        return;
    }
    isRestoring_ = restoring;
    emit isRestoringChanged();
}

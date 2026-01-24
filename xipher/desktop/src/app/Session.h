#pragma once

#include <QObject>
#include <QString>

class Session : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isAuthed READ isAuthed NOTIFY isAuthedChanged)
    Q_PROPERTY(bool isRestoring READ isRestoring NOTIFY isRestoringChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)

public:
    explicit Session(QObject* parent = nullptr);

    bool isAuthed() const;
    bool isRestoring() const;
    QString userId() const;
    QString username() const;
    QString token() const;

    void setSession(const QString& userId, const QString& username, const QString& token);
    void clear();
    void setRestoring(bool restoring);

signals:
    void isAuthedChanged();
    void isRestoringChanged();
    void userIdChanged();
    void usernameChanged();

private:
    bool isAuthed_ = false;
    bool isRestoring_ = false;
    QString userId_;
    QString username_;
    QString token_;
};

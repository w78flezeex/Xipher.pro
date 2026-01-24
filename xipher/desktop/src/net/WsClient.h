#pragma once

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QTimer>
#include <QWebSocket>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "models/Message.h"

class CookieJar;

struct TypingEvent {
    QString chatType;
    QString chatId;
    QString fromUserId;
    QString fromUsername;
    bool isTyping = false;
};

Q_DECLARE_METATYPE(TypingEvent)

class WsClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)

public:
    explicit WsClient(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent = nullptr);

    void connectToServer();
    void disconnectFromServer();

    void sendTyping(const QString& chatId, const QString& chatType, bool isTyping);
    void sendReceipt(const QString& type, const QString& messageId);

    bool isConnected() const;
    QString state() const;

signals:
    void connectionStateChanged(bool connected);
    void stateChanged();
    void authSucceeded();
    void authFailed(const QString& error);
    void newMessage(const Message& message);
    void typingReceived(const TypingEvent& event);
    void messageDelivered(const QString& messageId, const QString& chatId, const QString& fromUserId);
    void messageRead(const QString& messageId, const QString& chatId, const QString& fromUserId);
    void messageDeleted(const QString& messageId, const QString& chatId);
    void avatarUpdated(const QString& userId, const QString& avatarUrl);

private:
    enum class ConnectionState {
        Offline,
        Connecting,
        Authed,
        Reconnecting
    };

    enum class AuthStage {
        CookieOnly,
        AuthMessage,
        HeaderToken
    };

    void scheduleReconnect();
    void resetReconnect();
    void sendJson(const QJsonObject& payload);
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onMessageReceived(const QString& message);
    void sendAuthMessage();
    void startAuthTimeout();
    void advanceAuthStage();
    void setState(ConnectionState state);

    QString token() const;
    QString buildWsUrl() const;
    QString tokenPlaceholder() const;

    AppConfig* config_ = nullptr;
    Session* session_ = nullptr;
    CookieJar* cookieJar_ = nullptr;
    QWebSocket socket_;
    QTimer reconnectTimer_;
    QTimer authTimer_;
    int reconnectAttempts_ = 0;
    bool authed_ = false;
    bool forceHeaderToken_ = false;
    bool suppressReconnect_ = false;
    AuthStage authStage_ = AuthStage::CookieOnly;
    ConnectionState state_ = ConnectionState::Offline;
    QQueue<QJsonObject> pendingMessages_;

    QHash<QString, qint64> typingLastSentMs_;
};

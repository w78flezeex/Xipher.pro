#include "net/WsClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

#include "logging/Log.h"
#include "models/ApiDtos.h"
#include "net/CookieJar.h"

WsClient::WsClient(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent)
    : QObject(parent), config_(config), session_(session), cookieJar_(cookieJar) {
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<TypingEvent>("TypingEvent");

    socket_.setParent(this);

    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, [this]() { connectToServer(); });

    authTimer_.setSingleShot(true);
    connect(&authTimer_, &QTimer::timeout, this, [this]() { advanceAuthStage(); });

    connect(&socket_, &QWebSocket::connected, this, &WsClient::onSocketConnected);
    connect(&socket_, &QWebSocket::disconnected, this, &WsClient::onSocketDisconnected);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &WsClient::onMessageReceived);
    connect(&socket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &WsClient::onSocketError);
}

void WsClient::connectToServer() {
    if (socket_.state() == QAbstractSocket::ConnectingState ||
        socket_.state() == QAbstractSocket::ConnectedState) {
        return;
    }

    const QString wsUrl = buildWsUrl();
    if (wsUrl.isEmpty()) {
        qCWarning(logWs) << "WS connect skipped: empty wsUrl";
        return;
    }

    QNetworkRequest request(QUrl(wsUrl));
    const QString cookieHeader = cookieJar_ ? cookieJar_->cookieHeaderForUrl(request.url()) : QString();
    const QString bearer = token();
    if (bearer.isEmpty() && cookieHeader.isEmpty()) {
        qCWarning(logWs) << "WS connect skipped: missing auth token";
        return;
    }
    if (!cookieHeader.isEmpty()) {
        request.setRawHeader("Cookie", cookieHeader.toUtf8());
    }
    if (forceHeaderToken_ && !bearer.isEmpty() && bearer != tokenPlaceholder()) {
        request.setRawHeader("Authorization", ("Bearer " + bearer).toUtf8());
    }

    qCInfo(logWs) << "WS connecting to" << wsUrl;
    setState(reconnectAttempts_ > 0 ? ConnectionState::Reconnecting : ConnectionState::Connecting);
    socket_.open(request);
}

void WsClient::disconnectFromServer() {
    reconnectTimer_.stop();
    authTimer_.stop();
    socket_.close();
}

void WsClient::sendTyping(const QString& chatId, const QString& chatType, bool isTyping) {
    if (chatId.isEmpty()) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QString key = chatType + ":" + chatId;
    if (isTyping) {
        const qint64 lastMs = typingLastSentMs_.value(key, 0);
        if (nowMs - lastMs < 2000) {
            return;
        }
        typingLastSentMs_[key] = nowMs;
    }

    QJsonObject payload;
    payload["type"] = "typing";
    payload["token"] = token();
    payload["chat_type"] = chatType.isEmpty() ? "chat" : chatType;
    payload["chat_id"] = chatId;
    payload["is_typing"] = isTyping ? "1" : "0";
    sendJson(payload);
}

void WsClient::sendReceipt(const QString& type, const QString& messageId) {
    if (type.isEmpty() || messageId.isEmpty()) {
        return;
    }
    QJsonObject payload;
    payload["type"] = type;
    payload["token"] = token();
    payload["message_id"] = messageId;
    sendJson(payload);
}

bool WsClient::isConnected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

QString WsClient::state() const {
    switch (state_) {
        case ConnectionState::Connecting:
            return "connecting";
        case ConnectionState::Authed:
            return "authed";
        case ConnectionState::Reconnecting:
            return "reconnecting";
        case ConnectionState::Offline:
        default:
            return "offline";
    }
}

void WsClient::scheduleReconnect() {
    if (reconnectTimer_.isActive()) {
        return;
    }
    const int base = config_ ? config_->wsReconnectBaseMs() : 1000;
    const int delay = qMin(30000, base * (1 << qMin(reconnectAttempts_, 5)));
    reconnectAttempts_++;
    reconnectTimer_.start(delay);
    qCInfo(logWs) << "WS reconnect scheduled in" << delay << "ms";
}

void WsClient::resetReconnect() {
    reconnectAttempts_ = 0;
    reconnectTimer_.stop();
}

void WsClient::sendJson(const QJsonObject& payload) {
    if (payload.isEmpty()) {
        return;
    }
    if (!isConnected() || !authed_) {
        pendingMessages_.enqueue(payload);
        return;
    }
    const QJsonDocument doc(payload);
    socket_.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void WsClient::onSocketConnected() {
    resetReconnect();
    authed_ = false;
    if (authStage_ != AuthStage::HeaderToken) {
        authStage_ = AuthStage::CookieOnly;
        forceHeaderToken_ = false;
    }
    if (authStage_ == AuthStage::AuthMessage || authStage_ == AuthStage::HeaderToken) {
        sendAuthMessage();
    }
    startAuthTimeout();
}

void WsClient::onSocketDisconnected() {
    authed_ = false;
    authTimer_.stop();
    setState(ConnectionState::Offline);
    emit connectionStateChanged(false);
    if (suppressReconnect_) {
        suppressReconnect_ = false;
        return;
    }
    scheduleReconnect();
}

void WsClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    qCWarning(logWs) << "WS error" << socket_.errorString();
    setState(ConnectionState::Offline);
    emit connectionStateChanged(false);
    scheduleReconnect();
}

void WsClient::onMessageReceived(const QString& message) {
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qCWarning(logWs) << "WS invalid JSON" << Log::redact(message);
        return;
    }

    const QJsonObject obj = doc.object();
    const QString type = obj.value("type").toString();

    if (type == "auth_success") {
        authed_ = true;
        authTimer_.stop();
        setState(ConnectionState::Authed);
        emit connectionStateChanged(true);
        emit authSucceeded();
        while (!pendingMessages_.isEmpty()) {
            sendJson(pendingMessages_.dequeue());
        }
        return;
    }
    if (type == "auth_error") {
        authed_ = false;
        emit authFailed(obj.value("error").toString());
        advanceAuthStage();
        return;
    }
    if (type == "new_message") {
        const Message msg = ApiDtos::parseMessageFromWs(obj, session_ ? session_->userId() : QString());
        emit newMessage(msg);
        return;
    }
    if (type == "typing") {
        TypingEvent event;
        event.chatType = obj.value("chat_type").toString();
        event.chatId = obj.value("chat_id").toString();
        event.fromUserId = obj.value("from_user_id").toString();
        event.fromUsername = obj.value("from_username").toString();
        const QJsonValue typingVal = obj.value("is_typing");
        event.isTyping = ApiDtos::toBool(typingVal, true);
        emit typingReceived(event);
        return;
    }
    if (type == "message_delivered" || type == "message_read") {
        const QString messageId = obj.value("message_id").toString();
        const QString chatId = obj.value("chat_id").toString();
        const QString fromUser = obj.value("from_user_id").toString();
        if (type == "message_delivered") {
            emit messageDelivered(messageId, chatId, fromUser);
        } else {
            emit messageRead(messageId, chatId, fromUser);
        }
        return;
    }
    if (type == "message_deleted") {
        emit messageDeleted(obj.value("message_id").toString(), obj.value("chat_id").toString());
        return;
    }
    if (type == "avatar_updated") {
        emit avatarUpdated(obj.value("user_id").toString(), obj.value("avatar_url").toString());
        return;
    }
}

QString WsClient::token() const {
    return session_ ? session_->token() : QString();
}

QString WsClient::buildWsUrl() const {
    if (!config_) {
        return QString();
    }
    return config_->wsUrl();
}

QString WsClient::tokenPlaceholder() const {
    return QStringLiteral("cookie");
}

void WsClient::sendAuthMessage() {
    const QString bearer = token();
    if (bearer.isEmpty()) {
        return;
    }
    QJsonObject authPayload;
    authPayload["type"] = "auth";
    authPayload["token"] = bearer;
    const QJsonDocument doc(authPayload);
    socket_.sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void WsClient::startAuthTimeout() {
    authTimer_.start(1200);
}

void WsClient::advanceAuthStage() {
    if (authed_) {
        return;
    }
    if (authStage_ == AuthStage::CookieOnly) {
        authStage_ = AuthStage::AuthMessage;
        sendAuthMessage();
        startAuthTimeout();
        return;
    }
    if (authStage_ == AuthStage::AuthMessage) {
        authStage_ = AuthStage::HeaderToken;
        forceHeaderToken_ = true;
        suppressReconnect_ = true;
        socket_.close();
        connectToServer();
        return;
    }
    setState(ConnectionState::Offline);
    scheduleReconnect();
}

void WsClient::setState(ConnectionState state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit stateChanged();
}

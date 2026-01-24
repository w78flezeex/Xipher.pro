#include <QtTest/QtTest>

#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocketServer>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "net/ApiClient.h"
#include "net/CookieJar.h"
#include "net/UploadManager.h"
#include "net/WsClient.h"

namespace {
const QString kToken = "mocktoken";

QString buildJsonResponse(const QJsonObject& obj, const QByteArray& setCookie = QByteArray()) {
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
    if (!setCookie.isEmpty()) {
        resp += "Set-Cookie: " + setCookie + "\r\n";
    }
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    resp += body;
    return QString::fromLatin1(resp);
}

QString extractCookieToken(const QHash<QByteArray, QByteArray>& headers) {
    const QByteArray cookie = headers.value("cookie");
    const QList<QByteArray> parts = cookie.split(';');
    for (const auto& part : parts) {
        const QByteArray trimmed = part.trimmed();
        if (trimmed.startsWith("xipher_token=")) {
            return QString::fromLatin1(trimmed.mid(strlen("xipher_token=")));
        }
    }
    return QString();
}
}  // namespace

class MockHttpServer : public QTcpServer {
    Q_OBJECT

public:
    explicit MockHttpServer(QObject* parent = nullptr) : QTcpServer(parent) {}

signals:
    void sendMessageReceived(const QString& messageId);

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        auto* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(socketDescriptor);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        buffers_[socket] = QByteArray();
    }

private:
    void onReadyRead(QTcpSocket* socket) {
        buffers_[socket].append(socket->readAll());
        const QByteArray& buffer = buffers_[socket];
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            return;
        }
        const QByteArray headersPart = buffer.left(headerEnd);
        const QList<QByteArray> lines = headersPart.split('\n');
        if (lines.isEmpty()) {
            return;
        }
        const QByteArray requestLine = lines.first().trimmed();
        const QList<QByteArray> requestParts = requestLine.split(' ');
        if (requestParts.size() < 2) {
            return;
        }
        const QByteArray path = requestParts.at(1);

        QHash<QByteArray, QByteArray> headers;
        int contentLength = 0;
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const int colon = line.indexOf(':');
            if (colon <= 0) {
                continue;
            }
            const QByteArray key = line.left(colon).trimmed().toLower();
            const QByteArray value = line.mid(colon + 1).trimmed();
            headers.insert(key, value);
            if (key == "content-length") {
                contentLength = value.toInt();
            }
        }
        const int bodyStart = headerEnd + 4;
        if (buffer.size() < bodyStart + contentLength) {
            return;
        }
        const QByteArray body = buffer.mid(bodyStart, contentLength);
        buffers_.remove(socket);

        QJsonObject payload;
        if (!body.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(body);
            if (doc.isObject()) {
                payload = doc.object();
            }
        }

        QString tokenFromBody = payload.value("token").toString();
        if (tokenFromBody == "cookie") {
            tokenFromBody.clear();
        }
        const QString tokenFromCookie = extractCookieToken(headers);
        const QString token = tokenFromBody.isEmpty() ? tokenFromCookie : tokenFromBody;

        if (path == "/api/login") {
            QJsonObject data;
            data.insert("user_id", "u1");
            data.insert("username", payload.value("username").toString());
            data.insert("token", kToken);
            data.insert("is_premium", "false");
            data.insert("premium_plan", "");
            data.insert("premium_expires_at", "");
            QJsonObject resp;
            resp.insert("success", true);
            resp.insert("message", "ok");
            resp.insert("data", data);
            const QByteArray cookie = "xipher_token=" + kToken.toLatin1() + "; Path=/; HttpOnly";
            socket->write(buildJsonResponse(resp, cookie).toLatin1());
        } else if (path == "/api/validate-token") {
            QJsonObject resp;
            if (token == kToken) {
                QJsonObject data;
                data.insert("user_id", "u1");
                data.insert("username", "alice");
                resp.insert("success", true);
                resp.insert("message", "ok");
                resp.insert("data", data);
            } else {
                resp.insert("success", false);
                resp.insert("message", "Invalid token");
            }
            socket->write(buildJsonResponse(resp).toLatin1());
        } else if (path == "/api/chats") {
            QJsonObject resp;
            if (token == kToken) {
                QJsonObject chat;
                chat.insert("id", "u2");
                chat.insert("name", "bob");
                chat.insert("display_name", "Bob");
                chat.insert("avatar", "B");
                chat.insert("avatar_url", "");
                chat.insert("lastMessage", "Hi");
                chat.insert("time", "10:00");
                chat.insert("unread", 0);
                chat.insert("online", true);
                chat.insert("last_activity", "");
                chat.insert("is_saved_messages", false);
                chat.insert("is_bot", false);
                chat.insert("is_premium", false);
                QJsonArray chats;
                chats.append(chat);
                resp.insert("success", true);
                resp.insert("chats", chats);
            } else {
                resp.insert("success", false);
                resp.insert("message", "Invalid token");
            }
            socket->write(buildJsonResponse(resp).toLatin1());
        } else if (path == "/api/messages") {
            QJsonObject resp;
            if (token == kToken) {
                QJsonObject msg;
                msg.insert("id", "m1");
                msg.insert("sender_id", "u2");
                msg.insert("sent", false);
                msg.insert("status", "sent");
                msg.insert("is_read", false);
                msg.insert("is_delivered", false);
                msg.insert("content", "Hello");
                msg.insert("message_type", "text");
                msg.insert("time", "10:01");
                QJsonArray messages;
                messages.append(msg);
                resp.insert("success", true);
                resp.insert("messages", messages);
            } else {
                resp.insert("success", false);
                resp.insert("message", "Invalid token");
            }
            socket->write(buildJsonResponse(resp).toLatin1());
        } else if (path == "/api/send-message") {
            QJsonObject resp;
            if (token == kToken) {
                resp.insert("success", true);
                resp.insert("message", "sent");
                resp.insert("message_id", "m2");
                resp.insert("temp_id", payload.value("temp_id").toString());
                resp.insert("created_at", "2024-01-01 10:02:00");
                resp.insert("time", "10:02");
                resp.insert("status", "sent");
                resp.insert("is_read", false);
                resp.insert("is_delivered", false);
                resp.insert("content", payload.value("content").toString());
                resp.insert("message_type", payload.value("message_type").toString("text"));
                resp.insert("file_path", payload.value("file_path").toString());
                resp.insert("file_name", payload.value("file_name").toString());
                resp.insert("file_size", payload.value("file_size").toInt());
                resp.insert("reply_to_message_id", payload.value("reply_to_message_id").toString());
                emit sendMessageReceived("m2");
            } else {
                resp.insert("success", false);
                resp.insert("message", "Invalid token");
            }
            socket->write(buildJsonResponse(resp).toLatin1());
        } else if (path == "/api/upload-file") {
            QJsonObject resp;
            if (token == kToken) {
                const QByteArray fileData = QByteArray::fromBase64(payload.value("file_data").toString().toLatin1());
                resp.insert("success", true);
                resp.insert("file_path", "/files/" + payload.value("file_name").toString());
                resp.insert("file_name", payload.value("file_name").toString());
                resp.insert("file_size", fileData.size());
            } else {
                resp.insert("success", false);
                resp.insert("message", "Invalid token");
            }
            socket->write(buildJsonResponse(resp).toLatin1());
        } else {
            QJsonObject resp;
            resp.insert("success", false);
            resp.insert("message", "Not found");
            socket->write(buildJsonResponse(resp).toLatin1());
        }
        socket->flush();
        socket->disconnectFromHost();
    }

    QHash<QTcpSocket*, QByteArray> buffers_;
};

class MockWsServer : public QObject {
    Q_OBJECT

public:
    explicit MockWsServer(QObject* parent = nullptr)
        : QObject(parent), server_("mock", QWebSocketServer::NonSecureMode, this) {
        connect(&server_, &QWebSocketServer::newConnection, this, &MockWsServer::onNewConnection);
    }

    bool listen(quint16 port) {
        return server_.listen(QHostAddress::LocalHost, port);
    }

    quint16 port() const { return server_.serverPort(); }

    void broadcast(const QJsonObject& obj) {
        const QString payload = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        for (auto* client : clients_) {
            client->sendTextMessage(payload);
        }
    }

private slots:
    void onNewConnection() {
        QWebSocket* socket = server_.nextPendingConnection();
        clients_.append(socket);
        connect(socket, &QWebSocket::textMessageReceived, this, [this, socket](const QString& msg) {
            QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
            if (!doc.isObject()) {
                return;
            }
            QJsonObject obj = doc.object();
            if (obj.value("type").toString() == "auth") {
                const QString token = obj.value("token").toString();
                if (token == kToken || token == "cookie") {
                    QJsonObject resp;
                    resp.insert("type", "auth_success");
                    resp.insert("user_id", "u1");
                    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
                } else {
                    QJsonObject resp;
                    resp.insert("type", "auth_error");
                    resp.insert("error", "Invalid token");
                    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
                }
            }
        });
        connect(socket, &QWebSocket::disconnected, this, [this, socket]() {
            clients_.removeAll(socket);
            socket->deleteLater();
        });
    }

private:
    QWebSocketServer server_;
    QList<QWebSocket*> clients_;
};

class IntegrationTests : public QObject {
    Q_OBJECT

private slots:
    void endToEnd();
};

void IntegrationTests::endToEnd() {
    MockWsServer wsServer;
    QVERIFY(wsServer.listen(0));

    MockHttpServer httpServer;
    QVERIFY(httpServer.listen(QHostAddress::LocalHost, 0));

    AppConfig config;
    config.setBaseUrl(QString("http://127.0.0.1:%1").arg(httpServer.serverPort()));
    config.setWsUrl(QString("ws://127.0.0.1:%1/ws").arg(wsServer.port()));

    Session session;
    CookieJar cookieJar;
    ApiClient api(&config, &session, &cookieJar);
    WsClient ws(&config, &session, &cookieJar);
    UploadManager uploads(&config, &session, &cookieJar);

    ApiDtos::LoginResponse loginResp;
    QEventLoop loginLoop;
    QObject::connect(&api, &ApiClient::loginFinished, &loginLoop,
                     [&](const ApiDtos::LoginResponse& resp) {
                         loginResp = resp;
                         loginLoop.quit();
                     });
    api.login("alice", "pass");
    QTimer::singleShot(2000, &loginLoop, &QEventLoop::quit);
    loginLoop.exec();
    QVERIFY(loginResp.success);
    session.setSession(loginResp.userId, loginResp.username, "cookie");

    QSignalSpy authSpy(&ws, &WsClient::authSucceeded);
    ws.connectToServer();
    QVERIFY(authSpy.wait(2000));

    ApiDtos::ChatListResponse chatsResp;
    QEventLoop chatsLoop;
    QObject::connect(&api, &ApiClient::chatsFetched, &chatsLoop,
                     [&](const ApiDtos::ChatListResponse& resp) {
                         chatsResp = resp;
                         chatsLoop.quit();
                     });
    api.fetchChats();
    QTimer::singleShot(2000, &chatsLoop, &QEventLoop::quit);
    chatsLoop.exec();
    QVERIFY(chatsResp.success);
    QCOMPARE(chatsResp.chats.size(), 1);

    ApiDtos::MessageListResponse messagesResp;
    QEventLoop messagesLoop;
    QObject::connect(&api, &ApiClient::messagesFetched, &messagesLoop,
                     [&](const QString&, const ApiDtos::MessageListResponse& resp) {
                         messagesResp = resp;
                         messagesLoop.quit();
                     });
    api.fetchMessages("u2");
    QTimer::singleShot(2000, &messagesLoop, &QEventLoop::quit);
    messagesLoop.exec();
    QVERIFY(messagesResp.success);
    QCOMPARE(messagesResp.messages.size(), 1);

    ApiDtos::SendMessageResponse sendResp;
    QEventLoop sendLoop;
    QObject::connect(&api, &ApiClient::messageSent, &sendLoop,
                     [&](const QString&, const ApiDtos::SendMessageResponse& resp) {
                         sendResp = resp;
                         sendLoop.quit();
                     });
    ApiClient::SendMessageRequest request;
    request.chatId = "u2";
    request.content = "Hello from test";
    request.messageType = "text";
    request.tempId = "temp1";
    api.sendMessage(request);
    QTimer::singleShot(2000, &sendLoop, &QEventLoop::quit);
    sendLoop.exec();
    QVERIFY(sendResp.success);

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write("hello");
    tmp.flush();

    ApiDtos::UploadFileResponse uploadResp;
    QEventLoop uploadLoop;
    QObject::connect(&uploads, &UploadManager::uploadFinished, &uploadLoop,
                     [&](const QString&, const ApiDtos::UploadFileResponse& resp, const QString&) {
                         uploadResp = resp;
                         uploadLoop.quit();
                     });
    UploadManager::UploadRequest uploadRequest;
    uploadRequest.tempId = "up1";
    uploadRequest.filePath = tmp.fileName();
    uploadRequest.fileName = "hello.txt";
    uploadRequest.messageType = "file";
    uploads.startUpload(uploadRequest);
    QTimer::singleShot(5000, &uploadLoop, &QEventLoop::quit);
    uploadLoop.exec();
    QVERIFY(uploadResp.success);

    QSignalSpy deliveredSpy(&ws, &WsClient::messageDelivered);
    QJsonObject delivered;
    delivered.insert("type", "message_delivered");
    delivered.insert("message_id", "m2");
    delivered.insert("chat_id", "u2");
    delivered.insert("from_user_id", "u2");
    wsServer.broadcast(delivered);
    QVERIFY(deliveredSpy.wait(2000));
}

QTEST_MAIN(IntegrationTests)
#include "test_integration.moc"

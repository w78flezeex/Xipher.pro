#include <QtTest/QtTest>

#include "models/ApiDtos.h"

class JsonParsingTests : public QObject {
    Q_OBJECT

private slots:
    void loginResponseParsing();
    void chatListParsing();
    void messageListParsing();
    void messageMissingFields();
    void uploadResponseParsing();
};

void JsonParsingTests::loginResponseParsing() {
    const QByteArray payload =
        "{\"success\":true,\"message\":\"OK\",\"data\":{"
        "\"user_id\":\"u1\",\"username\":\"bob\",\"token\":\"t1\","
        "\"is_premium\":\"true\",\"premium_plan\":\"pro\"}}";

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());
    const ApiDtos::LoginResponse res = ApiDtos::parseLoginResponse(doc.object(), 200);
    QVERIFY(res.success);
    QCOMPARE(res.userId, QString("u1"));
    QCOMPARE(res.username, QString("bob"));
    QCOMPARE(res.token, QString("t1"));
    QVERIFY(res.isPremium);
    QCOMPARE(res.premiumPlan, QString("pro"));
}

void JsonParsingTests::chatListParsing() {
    const QByteArray payload =
        "{\"success\":true,\"chats\":[{" \
        "\"id\":\"c1\",\"name\":\"alice\",\"display_name\":\"Alice\","
        "\"lastMessage\":\"Hi\",\"time\":\"12:00\",\"unread\":2," \
        "\"online\":true,\"is_saved_messages\":false}]}";

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());
    const ApiDtos::ChatListResponse res = ApiDtos::parseChatListResponse(doc.object(), 200);
    QVERIFY(res.success);
    QCOMPARE(res.chats.size(), 1);
    QCOMPARE(res.chats[0].id, QString("c1"));
    QCOMPARE(res.chats[0].displayName, QString("Alice"));
    QCOMPARE(res.chats[0].unread, 2);
    QVERIFY(res.chats[0].online);
}

void JsonParsingTests::messageListParsing() {
    const QByteArray payload =
        "{\"success\":true,\"messages\":[{" \
        "\"id\":\"m1\",\"sender_id\":\"u1\",\"sent\":true," \
        "\"status\":\"sent\",\"is_read\":false,\"is_delivered\":true," \
        "\"content\":\"Hello\",\"message_type\":\"text\",\"time\":\"12:01\"}]}";

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());
    const ApiDtos::MessageListResponse res = ApiDtos::parseMessageListResponse(doc.object(), 200);
    QVERIFY(res.success);
    QCOMPARE(res.messages.size(), 1);
    QCOMPARE(res.messages[0].id, QString("m1"));
    QCOMPARE(res.messages[0].content, QString("Hello"));
    QVERIFY(res.messages[0].sent);
    QVERIFY(res.messages[0].isDelivered);
}

void JsonParsingTests::messageMissingFields() {
    const QByteArray payload =
        "{\"success\":true,\"messages\":[{\"id\":\"m2\",\"sender_id\":\"u2\"}]}";

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());
    const ApiDtos::MessageListResponse res = ApiDtos::parseMessageListResponse(doc.object(), 200);
    QVERIFY(res.success);
    QCOMPARE(res.messages.size(), 1);
    QCOMPARE(res.messages[0].id, QString("m2"));
    QCOMPARE(res.messages[0].messageType, QString("text"));
}

void JsonParsingTests::uploadResponseParsing() {
    const QByteArray payload =
        "{\"success\":true,\"file_path\":\"/files/f.txt\",\"file_name\":\"f.txt\",\"file_size\":42}";

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    QVERIFY(doc.isObject());
    const ApiDtos::UploadFileResponse res = ApiDtos::parseUploadFileResponse(doc.object(), 200);
    QVERIFY(res.success);
    QCOMPARE(res.filePath, QString("/files/f.txt"));
    QCOMPARE(res.fileName, QString("f.txt"));
    QCOMPARE(res.fileSize, 42);
}

QTEST_MAIN(JsonParsingTests)
#include "test_json_parsing.moc"

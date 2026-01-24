#include <QtTest/QtTest>

#include "net/ApiClient.h"
#include "logging/Log.h"

class UrlBuildingTests : public QObject {
    Q_OBJECT

private slots:
    void buildUrl();
    void redactSensitive();
    void redactHeaders();
};

void UrlBuildingTests::buildUrl() {
    const QUrl url1 = ApiClient::buildUrl("http://localhost:21971", "/api/login");
    QCOMPARE(url1.toString(), QString("http://localhost:21971/api/login"));

    const QUrl url2 = ApiClient::buildUrl("http://localhost:21971/", "api/messages");
    QCOMPARE(url2.toString(), QString("http://localhost:21971/api/messages"));
}

void UrlBuildingTests::redactSensitive() {
    const QString raw = "{\"token\":\"secret\",\"password\":\"pass\"} cookie=xipher_token=abc";
    const QString redacted = Log::redact(raw);
    QVERIFY(!redacted.contains("secret"));
    QVERIFY(!redacted.contains("pass"));
    QVERIFY(!redacted.contains("abc"));
}

void UrlBuildingTests::redactHeaders() {
    QList<QPair<QByteArray, QByteArray>> headers;
    headers.append({"Authorization", "Bearer secret"});
    headers.append({"Cookie", "xipher_token=abc"});
    headers.append({"X-Other", "ok"});

    const QString out = Log::redactHeaders(headers);
    QVERIFY(!out.contains("secret"));
    QVERIFY(!out.contains("abc"));
    QVERIFY(out.contains("X-Other"));
}

QTEST_MAIN(UrlBuildingTests)
#include "test_url_building.moc"

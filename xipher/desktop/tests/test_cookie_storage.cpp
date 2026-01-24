#include <QtTest/QtTest>

#include "net/CookieJar.h"
#include <QUrl>

class CookieStorageTests : public QObject {
    Q_OBJECT

private slots:
    void serializeRoundTrip();
};

void CookieStorageTests::serializeRoundTrip() {
    CookieJar jar;
    jar.setSessionCookie("http://localhost:21971", "token123");
    const QByteArray blob = jar.serialize();
    QVERIFY(!blob.isEmpty());

    CookieJar jar2;
    jar2.deserialize(blob);
    const QString header = jar2.cookieHeaderForUrl(QUrl("http://localhost:21971"));
    QVERIFY(header.contains("xipher_token"));
    QCOMPARE(jar2.sessionTokenForUrl(QUrl("http://localhost:21971")), QString("token123"));
}

QTEST_MAIN(CookieStorageTests)
#include "test_cookie_storage.moc"

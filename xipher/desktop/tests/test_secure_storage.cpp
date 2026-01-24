#include <QtTest/QtTest>

#include <QHash>
#include <QTemporaryDir>

#include "app/FileSecureStorage.h"
#include "app/SessionStore.h"

class MemorySecureStorage : public ISecureStorage {
public:
    bool setSecret(const QString& key, const QByteArray& value) override {
        data_[key] = value;
        return true;
    }

    std::optional<QByteArray> getSecret(const QString& key) override {
        if (!data_.contains(key)) {
            return std::nullopt;
        }
        return data_.value(key);
    }

    bool deleteSecret(const QString& key) override {
        data_.remove(key);
        return true;
    }

private:
    QHash<QString, QByteArray> data_;
};

class SecureStorageTests : public QObject {
    Q_OBJECT

private slots:
    void sessionRoundTrip();
    void migrateLegacy();
};

void SecureStorageTests::sessionRoundTrip() {
    MemorySecureStorage storage;
    SessionStore store(&storage);

    SessionInfo info;
    info.userId = "u1";
    info.username = "alice";
    info.token = "cookie";
    info.baseUrl = "http://localhost:21971";
    QVERIFY(store.saveSession(info));

    const QByteArray cookies = QByteArray("cookie_blob");
    QVERIFY(store.saveCookies(cookies));

    const std::optional<SessionInfo> loaded = store.loadSession();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->userId, info.userId);
    QCOMPARE(loaded->username, info.username);
    QCOMPARE(loaded->token, info.token);
    QCOMPARE(loaded->baseUrl, info.baseUrl);

    const std::optional<QByteArray> loadedCookies = store.loadCookies();
    QVERIFY(loadedCookies.has_value());
    QCOMPARE(*loadedCookies, cookies);
}

void SecureStorageTests::migrateLegacy() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString legacyPath = dir.filePath("legacy_store.json");

    FileSecureStorage legacyStorage(legacyPath);
    SessionStore legacyStore(&legacyStorage);

    SessionInfo info;
    info.userId = "u2";
    info.username = "bob";
    info.token = "cookie";
    info.baseUrl = "http://localhost:21971";
    QVERIFY(legacyStore.saveSession(info));
    QVERIFY(legacyStore.saveCookies(QByteArray("cookie_blob_2")));

    MemorySecureStorage secureStorage;
    QVERIFY(SessionStore::migrateLegacy(&legacyStorage, &secureStorage));

    SessionStore secureStore(&secureStorage);
    const std::optional<SessionInfo> loaded = secureStore.loadSession();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->userId, info.userId);

    const std::optional<QByteArray> loadedCookies = secureStore.loadCookies();
    QVERIFY(loadedCookies.has_value());
    QCOMPARE(*loadedCookies, QByteArray("cookie_blob_2"));

    QVERIFY(!legacyStore.loadSession().has_value());
    QVERIFY(!legacyStore.loadCookies().has_value());
}

QTEST_MAIN(SecureStorageTests)
#include "test_secure_storage.moc"

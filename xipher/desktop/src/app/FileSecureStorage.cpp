#include "app/FileSecureStorage.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <optional>

FileSecureStorage::FileSecureStorage(const QString& storePath) : storePath_(storePath) {}

bool FileSecureStorage::setSecret(const QString& key, const QByteArray& value) {
    QJsonObject obj;
    load(&obj);
    obj.insert(key, QString::fromLatin1(value.toBase64()));
    return save(obj);
}

std::optional<QByteArray> FileSecureStorage::getSecret(const QString& key) {
    QJsonObject obj;
    if (!load(&obj)) {
        return std::nullopt;
    }
    const QString encoded = obj.value(key).toString();
    if (encoded.isEmpty()) {
        return std::nullopt;
    }
    return QByteArray::fromBase64(encoded.toLatin1());
}

bool FileSecureStorage::deleteSecret(const QString& key) {
    QJsonObject obj;
    if (!load(&obj)) {
        return false;
    }
    obj.remove(key);
    return save(obj);
}

bool FileSecureStorage::load(QJsonObject* out) {
    if (!out) {
        return false;
    }
    QFile file(storePath_);
    if (!file.exists()) {
        *out = QJsonObject();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        *out = QJsonObject();
        return true;
    }
    *out = doc.object();
    return true;
}

bool FileSecureStorage::save(const QJsonObject& obj) {
    QSaveFile file(storePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        return false;
    }
    QFile::setPermissions(storePath_, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

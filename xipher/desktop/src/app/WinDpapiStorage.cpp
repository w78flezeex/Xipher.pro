#include "app/WinDpapiStorage.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <windows.h>
#include <wincrypt.h>

WinDpapiStorage::WinDpapiStorage(const QString& storePath) : storePath_(storePath) {}

bool WinDpapiStorage::setSecret(const QString& key, const QByteArray& value) {
    QJsonObject obj;
    load(&obj);

    QByteArray protectedValue;
    if (!protect(value, &protectedValue)) {
        return false;
    }
    obj.insert(key, QString::fromLatin1(protectedValue.toBase64()));
    return save(obj);
}

std::optional<QByteArray> WinDpapiStorage::getSecret(const QString& key) {
    QJsonObject obj;
    if (!load(&obj)) {
        return std::nullopt;
    }
    const QString encoded = obj.value(key).toString();
    if (encoded.isEmpty()) {
        return std::nullopt;
    }
    const QByteArray cipher = QByteArray::fromBase64(encoded.toLatin1());
    QByteArray plain;
    if (!unprotect(cipher, &plain)) {
        return std::nullopt;
    }
    return plain;
}

bool WinDpapiStorage::deleteSecret(const QString& key) {
    QJsonObject obj;
    if (!load(&obj)) {
        return false;
    }
    obj.remove(key);
    return save(obj);
}

bool WinDpapiStorage::load(QJsonObject* out) const {
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

bool WinDpapiStorage::save(const QJsonObject& obj) const {
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

bool WinDpapiStorage::protect(const QByteArray& plain, QByteArray* out) const {
    if (!out) {
        return false;
    }
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));
    inBlob.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB outBlob;
    if (!CryptProtectData(&inBlob, L"XipherDesktop", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        return false;
    }
    QByteArray result(reinterpret_cast<char*>(outBlob.pbData), static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    *out = result;
    return true;
}

bool WinDpapiStorage::unprotect(const QByteArray& cipher, QByteArray* out) const {
    if (!out) {
        return false;
    }
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(cipher.data()));
    inBlob.cbData = static_cast<DWORD>(cipher.size());

    DATA_BLOB outBlob;
    if (!CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        return false;
    }
    QByteArray result(reinterpret_cast<char*>(outBlob.pbData), static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    *out = result;
    return true;
}

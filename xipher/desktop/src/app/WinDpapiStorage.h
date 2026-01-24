#pragma once

#include "app/ISecureStorage.h"

#include <QString>
#include <QJsonObject>

class WinDpapiStorage : public ISecureStorage {
public:
    explicit WinDpapiStorage(const QString& storePath);

    bool setSecret(const QString& key, const QByteArray& value) override;
    std::optional<QByteArray> getSecret(const QString& key) override;
    bool deleteSecret(const QString& key) override;

private:
    bool load(QJsonObject* out) const;
    bool save(const QJsonObject& obj) const;
    bool protect(const QByteArray& plain, QByteArray* out) const;
    bool unprotect(const QByteArray& cipher, QByteArray* out) const;

    QString storePath_;
};

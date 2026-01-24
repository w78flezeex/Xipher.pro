#pragma once

#include "app/ISecureStorage.h"

#include <QJsonObject>

class FileSecureStorage : public ISecureStorage {
public:
    explicit FileSecureStorage(const QString& storePath);

    bool setSecret(const QString& key, const QByteArray& value) override;
    std::optional<QByteArray> getSecret(const QString& key) override;
    bool deleteSecret(const QString& key) override;

private:
    bool load(QJsonObject* out);
    bool save(const QJsonObject& obj);

    QString storePath_;
};

#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

class ISecureStorage {
public:
    virtual ~ISecureStorage() = default;
    virtual bool setSecret(const QString& key, const QByteArray& value) = 0;
    virtual std::optional<QByteArray> getSecret(const QString& key) = 0;
    virtual bool deleteSecret(const QString& key) = 0;
};

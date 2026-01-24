#pragma once

#include "app/ISecureStorage.h"

#include <QString>

class MacKeychainStorage : public ISecureStorage {
public:
    explicit MacKeychainStorage(const QString& serviceName);

    bool setSecret(const QString& key, const QByteArray& value) override;
    std::optional<QByteArray> getSecret(const QString& key) override;
    bool deleteSecret(const QString& key) override;

private:
    QString serviceName_;
};

#pragma once

#include "app/ISecureStorage.h"

#include <memory>
#include <optional>

class SecureStorage : public ISecureStorage {
public:
    SecureStorage(const QString& appId, const QString& encryptedStorePath);

    bool setSecret(const QString& key, const QByteArray& value) override;
    std::optional<QByteArray> getSecret(const QString& key) override;
    bool deleteSecret(const QString& key) override;

private:
    std::unique_ptr<ISecureStorage> backend_;
};

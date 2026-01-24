#include "app/SecureStorage.h"

#if defined(Q_OS_WIN)
#include "app/WinDpapiStorage.h"
#elif defined(Q_OS_MACOS)
#include "app/MacKeychainStorage.h"
#elif defined(Q_OS_LINUX)
#include "app/LinuxSecretStorage.h"
#endif

SecureStorage::SecureStorage(const QString& appId, const QString& encryptedStorePath) {
#if defined(Q_OS_WIN)
    backend_ = std::make_unique<WinDpapiStorage>(encryptedStorePath);
#elif defined(Q_OS_MACOS)
    backend_ = std::make_unique<MacKeychainStorage>(appId);
#elif defined(Q_OS_LINUX)
    backend_ = std::make_unique<LinuxSecretStorage>(appId);
#else
    Q_UNUSED(appId);
    Q_UNUSED(encryptedStorePath);
    backend_.reset();
#endif
}

bool SecureStorage::setSecret(const QString& key, const QByteArray& value) {
    if (!backend_) {
        return false;
    }
    return backend_->setSecret(key, value);
}

std::optional<QByteArray> SecureStorage::getSecret(const QString& key) {
    if (!backend_) {
        return std::nullopt;
    }
    return backend_->getSecret(key);
}

bool SecureStorage::deleteSecret(const QString& key) {
    if (!backend_) {
        return false;
    }
    return backend_->deleteSecret(key);
}

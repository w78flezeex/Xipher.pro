#include "app/MacKeychainStorage.h"

#include <Security/Security.h>

MacKeychainStorage::MacKeychainStorage(const QString& serviceName) : serviceName_(serviceName) {}

bool MacKeychainStorage::setSecret(const QString& key, const QByteArray& value) {
    deleteSecret(key);

    const QByteArray service = serviceName_.toUtf8();
    const QByteArray account = key.toUtf8();
    OSStatus status = SecKeychainAddGenericPassword(nullptr, static_cast<UInt32>(service.size()), service.constData(),
                                                    static_cast<UInt32>(account.size()), account.constData(),
                                                    static_cast<UInt32>(value.size()), value.constData(), nullptr);
    return status == errSecSuccess;
}

std::optional<QByteArray> MacKeychainStorage::getSecret(const QString& key) {
    const QByteArray service = serviceName_.toUtf8();
    const QByteArray account = key.toUtf8();
    void* data = nullptr;
    UInt32 dataLen = 0;
    SecKeychainItemRef item = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(service.size()), service.constData(),
                                                     static_cast<UInt32>(account.size()), account.constData(),
                                                     &dataLen, &data, &item);
    if (status == errSecItemNotFound) {
        return std::nullopt;
    }
    if (status != errSecSuccess) {
        if (item) {
            CFRelease(item);
        }
        return std::nullopt;
    }
    QByteArray result(static_cast<const char*>(data), static_cast<int>(dataLen));
    SecKeychainItemFreeContent(nullptr, data);
    if (item) {
        CFRelease(item);
    }
    return result;
}

bool MacKeychainStorage::deleteSecret(const QString& key) {
    const QByteArray service = serviceName_.toUtf8();
    const QByteArray account = key.toUtf8();
    void* data = nullptr;
    UInt32 dataLen = 0;
    SecKeychainItemRef item = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(nullptr, static_cast<UInt32>(service.size()), service.constData(),
                                                     static_cast<UInt32>(account.size()), account.constData(),
                                                     &dataLen, &data, &item);
    if (status == errSecItemNotFound) {
        return true;
    }
    if (status != errSecSuccess) {
        return false;
    }
    SecKeychainItemFreeContent(nullptr, data);
    if (!item) {
        return true;
    }
    OSStatus delStatus = SecKeychainItemDelete(item);
    CFRelease(item);
    return delStatus == errSecSuccess;
}

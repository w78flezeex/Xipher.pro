#include "app/LinuxSecretStorage.h"

#include <libsecret/secret.h>

namespace {
const SecretSchema kXipherSchema = {
    "com.xipher.desktop",
    SECRET_SCHEMA_NONE,
    {{"key", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, 0}}
};
}  // namespace

LinuxSecretStorage::LinuxSecretStorage(const QString& serviceName) : serviceName_(serviceName) {}

bool LinuxSecretStorage::setSecret(const QString& key, const QByteArray& value) {
    GError* error = nullptr;
    const QByteArray encoded = value.toBase64();
    const QByteArray keyUtf8 = key.toUtf8();
    const QByteArray label = (serviceName_ + ":" + key).toUtf8();
    const gboolean ok =
        secret_password_store_sync(&kXipherSchema, SECRET_COLLECTION_DEFAULT, label.constData(),
                                   encoded.constData(), nullptr, &error, "key", keyUtf8.constData(), nullptr);
    if (!ok) {
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    return true;
}

std::optional<QByteArray> LinuxSecretStorage::getSecret(const QString& key) {
    GError* error = nullptr;
    const QByteArray keyUtf8 = key.toUtf8();
    gchar* secret = secret_password_lookup_sync(&kXipherSchema, nullptr, &error, "key", keyUtf8.constData(), nullptr);
    if (error) {
        g_error_free(error);
        return std::nullopt;
    }
    if (!secret) {
        return std::nullopt;
    }
    const QByteArray decoded = QByteArray::fromBase64(QByteArray(secret));
    secret_password_free(secret);
    return decoded;
}

bool LinuxSecretStorage::deleteSecret(const QString& key) {
    GError* error = nullptr;
    const QByteArray keyUtf8 = key.toUtf8();
    const gboolean ok =
        secret_password_clear_sync(&kXipherSchema, nullptr, &error, "key", keyUtf8.constData(), nullptr);
    if (!ok) {
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    return true;
}

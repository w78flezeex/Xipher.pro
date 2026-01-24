#include "logging/Log.h"

#include <QRegularExpression>

Q_LOGGING_CATEGORY(logApp, "xipher.app")
Q_LOGGING_CATEGORY(logNet, "xipher.net")
Q_LOGGING_CATEGORY(logWs, "xipher.ws")

QString Log::redact(const QString& input) {
    QString output = input;

    static const QRegularExpression tokenRegex("\\\"token\\\"\\s*:\\s*\\\"[^\\\"]*\\\"");
    static const QRegularExpression passwordRegex("\\\"password\\\"\\s*:\\s*\\\"[^\\\"]*\\\"");
    static const QRegularExpression cookieRegex("(xipher_token=)([^;\\s]+)");
    static const QRegularExpression bearerRegex("Bearer\\s+[A-Za-z0-9\\-._~+/]+=*");

    output.replace(tokenRegex, "\"token\":\"***\"");
    output.replace(passwordRegex, "\"password\":\"***\"");
    output.replace(cookieRegex, "\\1***");
    output.replace(bearerRegex, "Bearer ***");

    return output;
}

QString Log::redactHeaders(const QList<QPair<QByteArray, QByteArray>>& headers) {
    QStringList items;
    for (const auto& header : headers) {
        const QString key = QString::fromLatin1(header.first).toLower();
        QString value = QString::fromLatin1(header.second);
        if (key == "cookie" || key == "set-cookie" || key == "authorization" ||
            key == "x-auth-token" || key == "x-session") {
            value = "***";
        }
        items << QString::fromLatin1(header.first) + ": " + value;
    }
    return items.join("; ");
}

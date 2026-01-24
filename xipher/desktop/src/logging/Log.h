#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logNet)
Q_DECLARE_LOGGING_CATEGORY(logWs)

class Log {
public:
    static QString redact(const QString& input);
    static QString redactHeaders(const QList<QPair<QByteArray, QByteArray>>& headers);
};

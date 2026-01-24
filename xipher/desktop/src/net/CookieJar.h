#pragma once

#include <QNetworkCookieJar>
#include <QString>
#include <QUrl>

class CookieJar : public QNetworkCookieJar {
    Q_OBJECT

public:
    explicit CookieJar(QObject* parent = nullptr);

    QString cookieHeaderForUrl(const QUrl& url) const;
    QString sessionTokenForUrl(const QUrl& url) const;
    static QString redactCookieHeader(const QString& header);
    void setSessionCookie(const QString& baseUrl, const QString& token);
    void clearSessionCookie();
    QByteArray serialize() const;
    void deserialize(const QByteArray& data);
};

#include "net/CookieJar.h"

#include <QNetworkCookie>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

CookieJar::CookieJar(QObject* parent) : QNetworkCookieJar(parent) {}

QString CookieJar::cookieHeaderForUrl(const QUrl& url) const {
    const QList<QNetworkCookie> cookies = cookiesForUrl(url);
    QStringList parts;
    parts.reserve(cookies.size());
    for (const auto& cookie : cookies) {
        parts << QString::fromLatin1(cookie.toRawForm(QNetworkCookie::NameAndValueOnly));
    }
    return parts.join("; ");
}

QString CookieJar::sessionTokenForUrl(const QUrl& url) const {
    const QList<QNetworkCookie> cookies = url.isValid() ? cookiesForUrl(url) : allCookies();
    for (const auto& cookie : cookies) {
        if (cookie.name() == "xipher_token" && !cookie.value().isEmpty()) {
            return QString::fromLatin1(cookie.value());
        }
    }
    return QString();
}

QString CookieJar::redactCookieHeader(const QString& header) {
    if (header.isEmpty()) {
        return header;
    }
    const QStringList parts = header.split(';');
    QStringList redacted;
    redacted.reserve(parts.size());
    for (const QString& part : parts) {
        const int eq = part.indexOf('=');
        if (eq == -1) {
            redacted << part.trimmed();
            continue;
        }
        const QString name = part.left(eq).trimmed();
        redacted << (name + "=***");
    }
    return redacted.join("; ");
}

void CookieJar::setSessionCookie(const QString& baseUrl, const QString& token) {
    if (baseUrl.isEmpty() || token.isEmpty()) {
        return;
    }
    QUrl url(baseUrl);
    if (!url.isValid()) {
        return;
    }
    QNetworkCookie cookie;
    cookie.setName("xipher_token");
    cookie.setValue(token.toUtf8());
    cookie.setPath("/");
    cookie.setDomain(url.host());
    cookie.setHttpOnly(true);
    if (url.scheme() == "https") {
        cookie.setSecure(true);
    }
    QList<QNetworkCookie> cookies = allCookies();
    cookies.append(cookie);
    setAllCookies(cookies);
}

void CookieJar::clearSessionCookie() {
    QList<QNetworkCookie> cookies = allCookies();
    cookies.erase(std::remove_if(cookies.begin(), cookies.end(),
                                 [](const QNetworkCookie& c) { return c.name() == "xipher_token"; }),
                  cookies.end());
    setAllCookies(cookies);
}

QByteArray CookieJar::serialize() const {
    QJsonArray array;
    for (const auto& cookie : allCookies()) {
        array.append(QString::fromLatin1(cookie.toRawForm()));
    }
    QJsonObject root;
    root.insert("cookies", array);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void CookieJar::deserialize(const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }
    const QJsonArray array = doc.object().value("cookies").toArray();
    QList<QNetworkCookie> cookies;
    cookies.reserve(array.size());
    for (const auto& entry : array) {
        const QByteArray raw = entry.toString().toLatin1();
        const QList<QNetworkCookie> parsed = QNetworkCookie::parseCookies(raw);
        for (const auto& cookie : parsed) {
            cookies.append(cookie);
        }
    }
    if (!cookies.isEmpty()) {
        setAllCookies(cookies);
    }
}

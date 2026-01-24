#pragma once

#include <QQmlNetworkAccessManagerFactory>
#include <QString>

class CachedNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory {
public:
    explicit CachedNetworkAccessManagerFactory(const QString& cacheDir);

    QNetworkAccessManager* create(QObject* parent) override;

private:
    QString cacheDir_;
};

#include "net/CachedNetworkAccessManagerFactory.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>

CachedNetworkAccessManagerFactory::CachedNetworkAccessManagerFactory(const QString& cacheDir)
    : cacheDir_(cacheDir) {}

QNetworkAccessManager* CachedNetworkAccessManagerFactory::create(QObject* parent) {
    auto* manager = new QNetworkAccessManager(parent);
    auto* cache = new QNetworkDiskCache(manager);
    QDir dir(cacheDir_);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    cache->setCacheDirectory(dir.absolutePath());
    cache->setMaximumCacheSize(100 * 1024 * 1024);
    manager->setCache(cache);
    return manager;
}

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class AppConfig : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString wsUrl READ wsUrl WRITE setWsUrl NOTIFY wsUrlChanged)
    Q_PROPERTY(int requestTimeoutMs READ requestTimeoutMs CONSTANT)
    Q_PROPERTY(int wsReconnectBaseMs READ wsReconnectBaseMs CONSTANT)
    Q_PROPERTY(qint64 uploadMaxBytes READ uploadMaxBytes WRITE setUploadMaxBytes NOTIFY uploadMaxBytesChanged)
    Q_PROPERTY(bool enableCalls READ enableCalls WRITE setEnableCalls NOTIFY enableCallsChanged)
    Q_PROPERTY(bool enableBots READ enableBots WRITE setEnableBots NOTIFY enableBotsChanged)
    Q_PROPERTY(bool enableAdmin READ enableAdmin WRITE setEnableAdmin NOTIFY enableAdminChanged)
    Q_PROPERTY(bool enableMarketplace READ enableMarketplace WRITE setEnableMarketplace NOTIFY enableMarketplaceChanged)
    Q_PROPERTY(bool callsEnabled READ callsEnabled WRITE setCallsEnabled NOTIFY callsEnabledChanged)

public:
    explicit AppConfig(QObject* parent = nullptr);

    QString baseUrl() const;
    void setBaseUrl(const QString& baseUrl);

    QString wsUrl() const;
    void setWsUrl(const QString& wsUrl);

    int requestTimeoutMs() const;
    int wsReconnectBaseMs() const;
    qint64 uploadMaxBytes() const;
    void setUploadMaxBytes(qint64 bytes);

    bool enableCalls() const;
    void setEnableCalls(bool enabled);

    bool enableBots() const;
    void setEnableBots(bool enabled);

    bool enableAdmin() const;
    void setEnableAdmin(bool enabled);

    bool enableMarketplace() const;
    void setEnableMarketplace(bool enabled);

    bool callsEnabled() const;
    void setCallsEnabled(bool enabled);

    QUrl buildUrl(const QString& path) const;

signals:
    void baseUrlChanged();
    void wsUrlChanged();
    void uploadMaxBytesChanged();
    void enableCallsChanged();
    void enableBotsChanged();
    void enableAdminChanged();
    void enableMarketplaceChanged();
    void callsEnabledChanged();

private:
    QString normalizeBaseUrl(const QString& baseUrl) const;
    QString buildWsUrlFromBase(const QString& baseUrl) const;

    QString baseUrl_;
    QString wsUrlOverride_;
    qint64 uploadMaxBytes_ = 25 * 1024 * 1024;
    bool enableCalls_ = false;
    bool enableBots_ = false;
    bool enableAdmin_ = false;
    bool enableMarketplace_ = false;
};

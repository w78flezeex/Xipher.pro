#pragma once

#include <QObject>
#include <QHash>
#include <QNetworkAccessManager>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "models/ApiDtos.h"

class CookieJar;
class QThread;

class UploadManager : public QObject {
    Q_OBJECT

public:
    struct UploadRequest {
        QString tempId;
        QString filePath;
        QString fileName;
        qint64 fileSize = 0;
        QString messageType;
    };

    explicit UploadManager(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent = nullptr);

    void startUpload(const UploadRequest& request);
    void cancelUpload(const QString& tempId);
    qint64 maxUploadBytes() const;

signals:
    void uploadProgress(const QString& tempId, int progress);
    void uploadFinished(const QString& tempId, const ApiDtos::UploadFileResponse& response, const QString& messageType);
    void uploadFailed(const QString& tempId, const ApiDtos::ApiError& error);
    void uploadCancelled(const QString& tempId);

private:
    struct Task {
        UploadRequest request;
        QThread* workerThread = nullptr;
        QObject* worker = nullptr;
        QNetworkReply* reply = nullptr;
        QByteArray base64Data;
        int attempt = 0;
        int maxAttempts = 3;
        bool cancelled = false;
    };

    void cleanupTask(const QString& tempId);
    void scheduleRetry(const QString& tempId);
    void beginNetworkUpload(const QString& tempId);
    QNetworkReply* postUpload(const UploadRequest& request, const QByteArray& base64Data);
    ApiDtos::ApiError buildError(QNetworkReply* reply, const QJsonObject& json) const;

    AppConfig* config_ = nullptr;
    Session* session_ = nullptr;
    CookieJar* cookieJar_ = nullptr;
    QNetworkAccessManager network_;
    QHash<QString, Task> tasks_;
};

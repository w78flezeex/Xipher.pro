#include "net/UploadManager.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

#include "logging/Log.h"
#include "net/CookieJar.h"

namespace {
class UploadWorker : public QObject {
    Q_OBJECT

public:
    UploadWorker(const QString& path, qint64 maxBytes) : path_(path), maxBytes_(maxBytes) {}

signals:
    void progress(int percent);
    void finished(const QByteArray& base64Data, const QString& error);

public slots:
    void run() {
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly)) {
            emit finished(QByteArray(), "Failed to open file");
            return;
        }
        const qint64 total = file.size();
        if (maxBytes_ > 0 && total > maxBytes_) {
            emit finished(QByteArray(), "File too large");
            return;
        }
        QByteArray data;
        if (total > 0) {
            data.reserve(static_cast<int>(qMin<qint64>(total, 50 * 1024 * 1024)));
        }
        qint64 read = 0;
        while (!file.atEnd()) {
            if (cancelled_) {
                emit finished(QByteArray(), "Cancelled");
                return;
            }
            const QByteArray chunk = file.read(64 * 1024);
            if (chunk.isEmpty() && file.error() != QFile::NoError) {
                emit finished(QByteArray(), "Failed to read file");
                return;
            }
            data.append(chunk);
            read += chunk.size();
            if (total > 0) {
                const int percent = static_cast<int>((read * 40) / total);
                emit progress(percent);
            }
        }
        if (cancelled_) {
            emit finished(QByteArray(), "Cancelled");
            return;
        }
        emit progress(40);
        const QByteArray base64 = data.toBase64();
        emit finished(base64, QString());
    }

    void cancel() { cancelled_ = true; }

private:
    QString path_;
    qint64 maxBytes_ = 0;
    bool cancelled_ = false;
};

QString formatLimitMessage(qint64 maxBytes) {
    if (maxBytes <= 0) {
        return QString("File too large");
    }
    const double mb = static_cast<double>(maxBytes) / (1024.0 * 1024.0);
    const int decimals = mb < 10.0 ? 1 : 0;
    return QString("File too large. Max %1 MB").arg(QString::number(mb, 'f', decimals));
}
}  // namespace

UploadManager::UploadManager(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent)
    : QObject(parent), config_(config), session_(session), cookieJar_(cookieJar) {
}

qint64 UploadManager::maxUploadBytes() const {
    return config_ ? config_->uploadMaxBytes() : 0;
}

void UploadManager::startUpload(const UploadRequest& request) {
    if (!config_ || !session_ || request.tempId.isEmpty() || request.filePath.isEmpty()) {
        ApiDtos::ApiError error;
        error.userMessage = "Invalid upload request";
        emit uploadFailed(request.tempId, error);
        return;
    }
    if (tasks_.contains(request.tempId)) {
        return;
    }
    QFileInfo info(request.filePath);
    if (!info.exists() || !info.isFile()) {
        ApiDtos::ApiError error;
        error.userMessage = "File not found";
        emit uploadFailed(request.tempId, error);
        return;
    }
    qint64 fileSize = request.fileSize > 0 ? request.fileSize : info.size();
    if (fileSize <= 0) {
        ApiDtos::ApiError error;
        error.userMessage = "File is empty";
        emit uploadFailed(request.tempId, error);
        return;
    }
    const qint64 limit = maxUploadBytes();
    if (limit > 0 && fileSize > limit) {
        ApiDtos::ApiError error;
        error.userMessage = formatLimitMessage(limit);
        emit uploadFailed(request.tempId, error);
        return;
    }

    Task task;
    task.request = request;
    task.request.fileSize = fileSize;
    task.workerThread = new QThread(this);
    auto* worker = new UploadWorker(request.filePath, limit);
    task.worker = worker;
    worker->moveToThread(task.workerThread);

    connect(task.workerThread, &QThread::started, worker, &UploadWorker::run);
    connect(worker, &UploadWorker::progress, this, [this, tempId = request.tempId](int percent) {
        emit uploadProgress(tempId, percent);
    });
    connect(worker, &UploadWorker::finished, this,
            [this, tempId = request.tempId](const QByteArray& base64, const QString& error) {
                if (error == "Cancelled") {
                    emit uploadCancelled(tempId);
                    cleanupTask(tempId);
                    return;
                }
                if (!error.isEmpty()) {
                    ApiDtos::ApiError apiError;
                    apiError.userMessage = error == "File too large" ? formatLimitMessage(maxUploadBytes()) : error;
                    emit uploadFailed(tempId, apiError);
                    cleanupTask(tempId);
                    return;
                }
                if (!tasks_.contains(tempId)) {
                    return;
                }
                Task& task = tasks_[tempId];
                task.base64Data = base64;
                beginNetworkUpload(tempId);
            });

    connect(task.workerThread, &QThread::finished, worker, &QObject::deleteLater);
    tasks_.insert(request.tempId, task);
    task.workerThread->start();
}

void UploadManager::cancelUpload(const QString& tempId) {
    if (!tasks_.contains(tempId)) {
        return;
    }
    Task& task = tasks_[tempId];
    task.cancelled = true;
    if (task.reply) {
        task.reply->abort();
    }
    auto* worker = qobject_cast<UploadWorker*>(task.worker);
    if (worker) {
        worker->cancel();
    }
}

void UploadManager::cleanupTask(const QString& tempId) {
    if (!tasks_.contains(tempId)) {
        return;
    }
    Task task = tasks_.take(tempId);
    if (task.workerThread) {
        task.workerThread->quit();
        task.workerThread->wait(2000);
        task.workerThread->deleteLater();
    }
}

void UploadManager::scheduleRetry(const QString& tempId) {
    if (!tasks_.contains(tempId)) {
        return;
    }
    Task& task = tasks_[tempId];
    if (task.cancelled) {
        cleanupTask(tempId);
        return;
    }
    const int baseMs = 800;
    const int attempt = qMax(1, task.attempt);
    const int delay = qMin(baseMs * (1 << (attempt - 1)), 5000) +
                      QRandomGenerator::global()->bounded(250);
    QTimer::singleShot(delay, this, [this, tempId]() { beginNetworkUpload(tempId); });
}

void UploadManager::beginNetworkUpload(const QString& tempId) {
    if (!tasks_.contains(tempId)) {
        return;
    }
    Task& task = tasks_[tempId];
    if (task.cancelled) {
        emit uploadCancelled(tempId);
        cleanupTask(tempId);
        return;
    }
    if (task.base64Data.isEmpty()) {
        ApiDtos::ApiError error;
        error.userMessage = "Upload payload missing";
        emit uploadFailed(tempId, error);
        cleanupTask(tempId);
        return;
    }
    if (task.attempt >= task.maxAttempts) {
        ApiDtos::ApiError error;
        error.userMessage = "Upload failed";
        emit uploadFailed(tempId, error);
        cleanupTask(tempId);
        return;
    }
    task.attempt += 1;
    task.reply = postUpload(task.request, task.base64Data);
    if (!task.reply) {
        ApiDtos::ApiError apiError;
        apiError.userMessage = "Failed to start upload";
        emit uploadFailed(tempId, apiError);
        cleanupTask(tempId);
        return;
    }
    emit uploadProgress(tempId, 40);
    connect(task.reply, &QNetworkReply::uploadProgress, this, [this, tempId](qint64 sent, qint64 total) {
        int progress = 40;
        if (total > 0) {
            progress = 40 + static_cast<int>((sent * 60) / total);
        }
        emit uploadProgress(tempId, progress);
    });
    connect(task.reply, &QNetworkReply::finished, this, [this, tempId]() {
        if (!tasks_.contains(tempId)) {
            return;
        }
        Task& task = tasks_[tempId];
        QNetworkReply* reply = task.reply;
        task.reply = nullptr;
        if (task.cancelled) {
            emit uploadCancelled(tempId);
            if (reply) {
                reply->deleteLater();
            }
            cleanupTask(tempId);
            return;
        }
        if (!reply) {
            ApiDtos::ApiError error;
            error.userMessage = "Upload failed";
            emit uploadFailed(tempId, error);
            cleanupTask(tempId);
            return;
        }
        const QByteArray body = reply->readAll();
        QJsonParseError jsonError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &jsonError);
        QJsonObject obj;
        if (jsonError.error == QJsonParseError::NoError && doc.isObject()) {
            obj = doc.object();
        }
        if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
            ApiDtos::ApiError error = buildError(reply, QJsonObject());
            if (error.userMessage.isEmpty()) {
                error.userMessage = "Invalid upload response";
            }
            if (error.isTransient && task.attempt < task.maxAttempts) {
                reply->deleteLater();
                scheduleRetry(tempId);
                return;
            }
            emit uploadFailed(tempId, error);
            reply->deleteLater();
            cleanupTask(tempId);
            return;
        }
        ApiDtos::UploadFileResponse response =
            ApiDtos::parseUploadFileResponse(obj,
                                             reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        if (!response.success) {
            response.error = buildError(reply, obj);
            if (response.error.isTransient && task.attempt < task.maxAttempts) {
                reply->deleteLater();
                scheduleRetry(tempId);
                return;
            }
            emit uploadFailed(tempId, response.error);
            reply->deleteLater();
            cleanupTask(tempId);
            return;
        }
        emit uploadFinished(tempId, response, task.request.messageType);
        reply->deleteLater();
        cleanupTask(tempId);
    });
}

QNetworkReply* UploadManager::postUpload(const UploadRequest& request, const QByteArray& base64Data) {
    if (!config_) {
        return nullptr;
    }
    const QString tokenValue = session_ ? session_->token() : QString();
    if (tokenValue.isEmpty()) {
        return nullptr;
    }
    const QUrl url = config_->buildUrl("/api/upload-file");
    if (!url.isValid()) {
        return nullptr;
    }
    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["file_data"] = QString::fromLatin1(base64Data);
    payload["file_name"] = request.fileName;

    QNetworkRequest requestObj(url);
    requestObj.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    requestObj.setTransferTimeout(config_->requestTimeoutMs());
    if (cookieJar_) {
        const QString cookieHeader = cookieJar_->cookieHeaderForUrl(url);
        if (!cookieHeader.isEmpty()) {
            requestObj.setRawHeader("Cookie", cookieHeader.toUtf8());
        }
    }
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    qCInfo(logNet) << "HTTP POST" << url.toString() << "upload bytes" << body.size();
    return network_.post(requestObj, body);
}

ApiDtos::ApiError UploadManager::buildError(QNetworkReply* reply, const QJsonObject& json) const {
    ApiDtos::ApiError error;
    if (!reply) {
        error.userMessage = "Upload failed";
        return error;
    }
    error.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        error.userMessage = "Network error";
        error.debugMessage = reply->errorString();
        error.isNetworkError = true;
        error.isTransient = true;
        return error;
    }
    if (error.httpStatus == 408 || error.httpStatus == 429 || error.httpStatus == 502 ||
        error.httpStatus == 503 || error.httpStatus == 504) {
        error.isTransient = true;
    }
    if (json.contains("success") && !json.value("success").toBool()) {
        error.userMessage = json.value("message").toString();
        if (error.userMessage.isEmpty()) {
            error.userMessage = "Upload failed";
        }
        error.serverCode = json.value("code").toString();
    }
    return error;
}

#include "UploadManager.moc"

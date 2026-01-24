#include "net/ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#include "logging/Log.h"
#include "net/CookieJar.h"

ApiClient::ApiClient(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent)
    : QObject(parent), config_(config), session_(session), cookieJar_(cookieJar) {
    if (cookieJar_) {
        network_.setCookieJar(cookieJar_);
    }
}

QString ApiClient::resolveUrl(const QString& path) const {
    if (path.isEmpty()) {
        return path;
    }
    const QString trimmed = path.trimmed();
    if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
        return trimmed;
    }
    if (!config_) {
        return trimmed;
    }
    const QUrl url = config_->buildUrl(trimmed);
    return url.isValid() ? url.toString() : trimmed;
}

void ApiClient::login(const QString& username, const QString& password) {
    QJsonObject payload;
    payload["username"] = username;
    payload["password"] = password;

    postJsonWithRetry("/api/login", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::LoginResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Network error" : raw.error.userMessage;
            response.error = raw.error;
            emit loginFinished(response);
            return;
        }
        ApiDtos::LoginResponse response = ApiDtos::parseLoginResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        emit loginFinished(response);
    });
}

void ApiClient::validateToken() {
    QJsonObject payload;
    const QString tokenValue = session_ ? session_->token() : QString();
    if (!tokenValue.isEmpty()) {
        payload["token"] = tokenValue;
    }

    postJsonWithRetry("/api/validate-token", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::SessionResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Network error" : raw.error.userMessage;
            response.error = raw.error;
            emit sessionValidated(response);
            return;
        }
        ApiDtos::SessionResponse response = ApiDtos::parseSessionResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        emit sessionValidated(response);
    });
}

void ApiClient::fetchChats() {
    QString tokenValue;
    if (!ensureToken("fetchChats", &tokenValue)) {
        ApiDtos::ChatListResponse response;
        response.success = false;
        response.message = "Missing token";
        response.error.userMessage = response.message;
        emit chatsFetched(response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;

    postJsonWithRetry("/api/chats", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::ChatListResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Network error" : raw.error.userMessage;
            response.error = raw.error;
            emit chatsFetched(response);
            return;
        }
        ApiDtos::ChatListResponse response = ApiDtos::parseChatListResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        emit chatsFetched(response);
    });
}

void ApiClient::fetchMessages(const QString& chatId) {
    QString tokenValue;
    if (!ensureToken("fetchMessages", &tokenValue)) {
        ApiDtos::MessageListResponse response;
        response.success = false;
        response.message = "Missing token";
        response.error.userMessage = response.message;
        emit messagesFetched(chatId, response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["friend_id"] = chatId;

    postJsonWithRetry("/api/messages", payload, 0, [this, chatId](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::MessageListResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Network error" : raw.error.userMessage;
            response.error = raw.error;
            emit messagesFetched(chatId, response);
            return;
        }
        ApiDtos::MessageListResponse response = ApiDtos::parseMessageListResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        emit messagesFetched(chatId, response);
    });
}

void ApiClient::sendMessage(const SendMessageRequest& request) {
    QString tokenValue;
    if (!ensureToken("sendMessage", &tokenValue)) {
        ApiDtos::SendMessageResponse response;
        response.success = false;
        response.message = "Missing token";
        response.error.userMessage = response.message;
        emit messageSent(request.chatId, response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["receiver_id"] = request.chatId;
    payload["content"] = request.content;
    payload["message_type"] = request.messageType.isEmpty() ? "text" : request.messageType;
    if (!request.replyToId.isEmpty()) {
        payload["reply_to_message_id"] = request.replyToId;
    }
    if (!request.tempId.isEmpty()) {
        payload["temp_id"] = request.tempId;
    }
    if (!request.filePath.isEmpty()) {
        payload["file_path"] = request.filePath;
        payload["file_name"] = request.fileName;
        payload["file_size"] = static_cast<double>(request.fileSize);
    }

    postJsonWithRetry("/api/send-message", payload, 0, [this, request](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::SendMessageResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Network error" : raw.error.userMessage;
            response.error = raw.error;
            response.messageData.tempId = request.tempId;
            emit messageSent(request.chatId, response);
            return;
        }
        ApiDtos::SendMessageResponse response = ApiDtos::parseSendMessageResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        if (response.messageData.tempId.isEmpty()) {
            response.messageData.tempId = request.tempId;
        }
        emit messageSent(request.chatId, response);
    });
}

void ApiClient::deleteMessage(const QString& messageId) {
    QString tokenValue;
    if (!ensureToken("deleteMessage", &tokenValue)) {
        ApiDtos::ApiError error;
        error.userMessage = "Missing token";
        emit messageDeleted(messageId, error);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["message_id"] = messageId;

    postJsonWithRetry("/api/delete-message", payload, 0, [this, messageId](const RawResponse& raw) {
        ApiDtos::ApiError error = raw.error;
        if (!raw.ok) {
            if (error.userMessage.isEmpty()) {
                error.userMessage = "Failed to delete";
            }
            emit messageDeleted(messageId, error);
            return;
        }
        const bool success = raw.json.value("success").toBool();
        if (!success) {
            error.userMessage = raw.json.value("message").toString("Failed to delete");
        }
        emit messageDeleted(messageId, error);
    });
}

void ApiClient::uploadFileBase64(const QString& fileName, const QByteArray& base64Data,
                                 const std::function<void(const ApiDtos::UploadFileResponse&)>& callback) {
    QString tokenValue;
    if (!ensureToken("uploadFile", &tokenValue)) {
        ApiDtos::UploadFileResponse response;
        response.success = false;
        response.message = "Missing token";
        response.error.userMessage = response.message;
        callback(response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["file_data"] = QString::fromLatin1(base64Data);
    payload["file_name"] = fileName;

    postJsonWithRetry("/api/upload-file", payload, 0, [callback](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::UploadFileResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Upload failed" : raw.error.userMessage;
            response.error = raw.error;
            callback(response);
            return;
        }
        ApiDtos::UploadFileResponse response = ApiDtos::parseUploadFileResponse(raw.json, raw.httpStatus);
        if (!response.success) {
            response.error = raw.error;
        }
        callback(response);
    });
}

void ApiClient::getUserProfile(const QString& userId) {
    QString tokenValue;
    if (!ensureToken("getUserProfile", &tokenValue)) {
        ApiDtos::UserProfileResponse response;
        response.success = false;
        response.message = "Missing token";
        emit userProfileFetched(response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["user_id"] = userId;

    postJsonWithRetry("/api/get-user-profile", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::UserProfileResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Failed to load profile" : raw.error.userMessage;
            response.error = raw.error;
            emit userProfileFetched(response);
            return;
        }
        ApiDtos::UserProfileResponse response = ApiDtos::parseUserProfileResponse(raw.json, raw.httpStatus);
        emit userProfileFetched(response);
    });
}

void ApiClient::updateUserProfile(const QString& firstName, const QString& lastName, const QString& bio) {
    QString tokenValue;
    if (!ensureToken("updateUserProfile", &tokenValue)) {
        emit userProfileUpdated(false, "Missing token");
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["first_name"] = firstName;
    payload["last_name"] = lastName;
    payload["bio"] = bio;

    postJsonWithRetry("/api/update-user-profile", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            emit userProfileUpdated(false, raw.error.userMessage.isEmpty() ? "Failed to save" : raw.error.userMessage);
            return;
        }
        const bool success = raw.json.value("success").toBool();
        const QString message = raw.json.value("message").toString();
        emit userProfileUpdated(success, message);
    });
}

void ApiClient::findUser(const QString& username) {
    QString tokenValue;
    if (!ensureToken("findUser", &tokenValue)) {
        ApiDtos::FindUserResponse response;
        response.success = false;
        response.message = "Missing token";
        emit userFound(response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["username"] = username;

    postJsonWithRetry("/api/find-user", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::FindUserResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "User not found" : raw.error.userMessage;
            response.error = raw.error;
            emit userFound(response);
            return;
        }
        ApiDtos::FindUserResponse response = ApiDtos::parseFindUserResponse(raw.json, raw.httpStatus);
        emit userFound(response);
    });
}

void ApiClient::createGroup(const QString& name, const QStringList& memberIds) {
    QString tokenValue;
    if (!ensureToken("createGroup", &tokenValue)) {
        ApiDtos::CreateGroupResponse response;
        response.success = false;
        response.message = "Missing token";
        emit groupCreated(response);
        return;
    }

    QJsonObject payload;
    payload["token"] = tokenValue;
    payload["name"] = name;
    
    QJsonArray members;
    for (const QString& id : memberIds) {
        members.append(id);
    }
    payload["members"] = members;

    postJsonWithRetry("/api/create-group", payload, 0, [this](const RawResponse& raw) {
        if (!raw.ok) {
            ApiDtos::CreateGroupResponse response;
            response.success = false;
            response.message = raw.error.userMessage.isEmpty() ? "Failed to create group" : raw.error.userMessage;
            response.error = raw.error;
            emit groupCreated(response);
            return;
        }
        ApiDtos::CreateGroupResponse response = ApiDtos::parseCreateGroupResponse(raw.json, raw.httpStatus);
        emit groupCreated(response);
    });
}

QUrl ApiClient::buildUrl(const QString& baseUrl, const QString& path) {
    QUrl url(baseUrl);
    if (!url.isValid()) {
        return QUrl();
    }
    QString cleaned = path.trimmed();
    if (!cleaned.startsWith('/')) {
        cleaned.prepend('/');
    }
    url.setPath(cleaned);
    return url;
}

void ApiClient::postJsonWithRetry(const QString& path, const QJsonObject& payload, int attempt,
                                  const std::function<void(const RawResponse&)>& callback) {
    postJsonOnce(path, payload, [this, path, payload, attempt, callback](const RawResponse& raw) {
        if (raw.ok || !raw.error.isTransient || attempt >= 2) {
            callback(raw);
            return;
        }
        const int delayMs = computeRetryDelayMs(attempt);
        QTimer::singleShot(delayMs, this, [this, path, payload, attempt, callback]() {
            postJsonWithRetry(path, payload, attempt + 1, callback);
        });
    });
}

void ApiClient::postJsonOnce(const QString& path, const QJsonObject& payload,
                             const std::function<void(const RawResponse&)>& callback) {
    if (!config_) {
        RawResponse raw;
        raw.ok = false;
        raw.error.userMessage = "Missing AppConfig";
        callback(raw);
        return;
    }

    const QUrl url = config_->buildUrl(path);
    if (!url.isValid()) {
        RawResponse raw;
        raw.ok = false;
        raw.error.userMessage = "Invalid base URL";
        callback(raw);
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(config_->requestTimeoutMs());

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qCInfo(logNet) << "HTTP POST" << url.toString() << "payload bytes" << body.size();
    QNetworkReply* reply = network_.post(request, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        RawResponse raw = parseReply(reply);
        reply->deleteLater();
        callback(raw);
    });
}

ApiClient::RawResponse ApiClient::parseReply(QNetworkReply* reply) {
    RawResponse raw;
    raw.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QByteArray body = reply->readAll();
    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
        raw.ok = false;
        raw.error = buildError(reply, QJsonObject());
        if (raw.error.userMessage.isEmpty()) {
            raw.error.userMessage = "Invalid JSON response";
        }
        raw.error.debugMessage = jsonError.errorString();
        return raw;
    }

    raw.ok = true;
    raw.json = doc.object();
    raw.error = buildError(reply, raw.json);
    return raw;
}

ApiDtos::ApiError ApiClient::buildError(QNetworkReply* reply, const QJsonObject& json) const {
    ApiDtos::ApiError error;
    error.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        error.userMessage = "Network error";
        error.debugMessage = reply->errorString();
        error.isNetworkError = true;
        error.isTransient = true;
        return error;
    }

    if (json.contains("success") && !json.value("success").toBool()) {
        error.userMessage = json.value("message").toString();
        if (error.userMessage.isEmpty()) {
            error.userMessage = "Request failed";
        }
        error.serverCode = json.value("code").toString();
        error.debugMessage = error.userMessage;
    }

    if (error.httpStatus == 408 || error.httpStatus == 429 || error.httpStatus == 502 ||
        error.httpStatus == 503 || error.httpStatus == 504) {
        error.isTransient = true;
    }

    return error;
}

bool ApiClient::ensureToken(const QString& context, QString* outToken) {
    if (!session_) {
        qCWarning(logNet) << context << "missing session";
        return false;
    }
    const QString tokenValue = session_->token();
    if (tokenValue.isEmpty()) {
        qCWarning(logNet) << context << "missing token";
        return false;
    }
    if (outToken) {
        *outToken = tokenValue;
    }
    return true;
}

int ApiClient::computeRetryDelayMs(int attempt) const {
    const int base = 500;
    const int cap = 3000;
    const int exp = qMin(cap, base * (1 << attempt));
    const int jitter = QRandomGenerator::global()->bounded(250);
    return exp + jitter;
}

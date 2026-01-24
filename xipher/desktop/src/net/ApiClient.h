#pragma once

#include <functional>
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "models/ApiDtos.h"

class CookieJar;

class ApiClient : public QObject {
    Q_OBJECT

public:
    struct SendMessageRequest {
        QString chatId;
        QString content;
        QString messageType;
        QString filePath;
        QString fileName;
        qint64 fileSize = 0;
        QString replyToId;
        QString tempId;
    };

    explicit ApiClient(AppConfig* config, Session* session, CookieJar* cookieJar, QObject* parent = nullptr);

    Q_INVOKABLE void login(const QString& username, const QString& password);
    Q_INVOKABLE void validateToken();
    Q_INVOKABLE void fetchChats();
    Q_INVOKABLE void fetchMessages(const QString& chatId);
    void sendMessage(const SendMessageRequest& request);
    void deleteMessage(const QString& messageId);
    void uploadFileBase64(const QString& fileName, const QByteArray& base64Data,
                          const std::function<void(const ApiDtos::UploadFileResponse&)>& callback);
    
    Q_INVOKABLE void getUserProfile(const QString& userId);
    Q_INVOKABLE void updateUserProfile(const QString& firstName, const QString& lastName, const QString& bio);
    Q_INVOKABLE void findUser(const QString& username);
    Q_INVOKABLE void createGroup(const QString& name, const QStringList& memberIds);

    QString resolveUrl(const QString& path) const;

    static QUrl buildUrl(const QString& baseUrl, const QString& path);

signals:
    void loginFinished(const ApiDtos::LoginResponse& response);
    void sessionValidated(const ApiDtos::SessionResponse& response);
    void chatsFetched(const ApiDtos::ChatListResponse& response);
    void messagesFetched(const QString& chatId, const ApiDtos::MessageListResponse& response);
    void messageSent(const QString& chatId, const ApiDtos::SendMessageResponse& response);
    void messageDeleted(const QString& messageId, const ApiDtos::ApiError& error);
    void userProfileFetched(const ApiDtos::UserProfileResponse& response);
    void userProfileUpdated(bool success, const QString& message);
    void userFound(const ApiDtos::FindUserResponse& response);
    void groupCreated(const ApiDtos::CreateGroupResponse& response);

private:
    struct RawResponse {
        bool ok = false;
        int httpStatus = 0;
        QJsonObject json;
        ApiDtos::ApiError error;
    };

    void postJsonWithRetry(const QString& path, const QJsonObject& payload, int attempt,
                           const std::function<void(const RawResponse&)>& callback);
    void postJsonOnce(const QString& path, const QJsonObject& payload,
                      const std::function<void(const RawResponse&)>& callback);
    RawResponse parseReply(QNetworkReply* reply);
    ApiDtos::ApiError buildError(QNetworkReply* reply, const QJsonObject& json) const;
    bool ensureToken(const QString& context, QString* outToken);
    int computeRetryDelayMs(int attempt) const;

    AppConfig* config_ = nullptr;
    Session* session_ = nullptr;
    CookieJar* cookieJar_ = nullptr;
    QNetworkAccessManager network_;
};

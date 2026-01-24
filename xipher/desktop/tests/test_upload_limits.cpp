#include <QtTest/QtTest>

#include <QEventLoop>
#include <QTemporaryFile>
#include <QTimer>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "net/CookieJar.h"
#include "net/UploadManager.h"

class UploadLimitTests : public QObject {
    Q_OBJECT

private slots:
    void rejectsLargeFile();
};

void UploadLimitTests::rejectsLargeFile() {
    AppConfig config;
    config.setUploadMaxBytes(1);
    Session session;
    CookieJar cookieJar;
    UploadManager manager(&config, &session, &cookieJar);

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    QByteArray data(5, 'a');
    QVERIFY(tmp.write(data) == data.size());
    tmp.flush();

    UploadManager::UploadRequest request;
    request.tempId = "up_limit";
    request.filePath = tmp.fileName();
    request.fileName = "big.txt";
    request.fileSize = data.size();
    request.messageType = "file";

    QEventLoop loop;
    QString errorMessage;
    QObject::connect(&manager, &UploadManager::uploadFailed, &loop,
                     [&errorMessage, &loop](const QString&, const ApiDtos::ApiError& error) {
                         errorMessage = error.userMessage;
                         loop.quit();
                     });

    manager.startUpload(request);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(!errorMessage.isEmpty());
    QVERIFY(errorMessage.contains("Max"));
}

QTEST_MAIN(UploadLimitTests)
#include "test_upload_limits.moc"

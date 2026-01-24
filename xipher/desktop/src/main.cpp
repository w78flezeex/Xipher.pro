#include <QCoreApplication>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml/qqml.h>

#include "app/AppConfig.h"
#include "app/Session.h"
#include "app/SessionManager.h"
#include "app/SettingsStore.h"
#include "net/ApiClient.h"
#include "net/CachedNetworkAccessManagerFactory.h"
#include "net/CookieJar.h"
#include "net/UploadManager.h"
#include "net/WsClient.h"
#include "services/FileDialogService.h"
#include "store/ChatStore.h"
#include "vm/AuthViewModel.h"
#include "vm/ShellViewModel.h"
#include "vm/SettingsViewModel.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
#ifndef QT_DEBUG
    QLoggingCategory::setFilterRules("xipher.*.debug=false\nxipher.*.info=true");
#endif

    AppConfig config;
    SettingsStore settings;

    const QString defaultBaseUrl = QString("https://xipher.pro");
    if (settings.baseUrl().isEmpty()) {
        settings.setBaseUrl(defaultBaseUrl);
    }
    config.setBaseUrl(settings.baseUrl());
    const QByteArray enableCalls = qgetenv("XIPHER_ENABLE_CALLS");
    if (enableCalls == "1" || enableCalls.toLower() == "true") {
        config.setEnableCalls(true);
    }
    const QByteArray enableBots = qgetenv("XIPHER_ENABLE_BOTS");
    if (enableBots == "1" || enableBots.toLower() == "true") {
        config.setEnableBots(true);
    }
    const QByteArray enableAdmin = qgetenv("XIPHER_ENABLE_ADMIN");
    if (enableAdmin == "1" || enableAdmin.toLower() == "true") {
        config.setEnableAdmin(true);
    }
    const QByteArray enableMarketplace = qgetenv("XIPHER_ENABLE_MARKETPLACE");
    if (enableMarketplace == "1" || enableMarketplace.toLower() == "true") {
        config.setEnableMarketplace(true);
    }
    const QByteArray uploadMaxMb = qgetenv("XIPHER_UPLOAD_MAX_MB");
    if (!uploadMaxMb.isEmpty()) {
        bool ok = false;
        const int mb = uploadMaxMb.toInt(&ok);
        if (ok && mb > 0) {
            config.setUploadMaxBytes(static_cast<qint64>(mb) * 1024 * 1024);
        }
    }

    QObject::connect(&settings, &SettingsStore::baseUrlChanged, &config,
                     [&settings, &config]() { config.setBaseUrl(settings.baseUrl()); });

    Session session;
    FileDialogService fileDialogs;
    auto* cookieJar = new CookieJar(&app);
    ApiClient api(&config, &session, cookieJar);
    WsClient ws(&config, &session, cookieJar);
    UploadManager uploads(&config, &session, cookieJar);
    ChatStore store(&api, &ws, &uploads, &session);
    SessionManager sessionManager(&session, &settings, &api, cookieJar);

    AuthViewModel authVm(&api, &sessionManager, &settings);
    ShellViewModel shellVm(&store);
    SettingsViewModel settingsVm(&settings, &config, &api, &session);

    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/qt/qml/Xipher/Desktop/theme/Theme.qml")),
                             "Xipher.Theme", 1, 0, "Theme");

    QQmlApplicationEngine engine;
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    engine.setNetworkAccessManagerFactory(new CachedNetworkAccessManagerFactory(cacheDir));
    engine.rootContext()->setContextProperty("AppConfig", &config);
    engine.rootContext()->setContextProperty("SettingsStore", &settings);
    engine.rootContext()->setContextProperty("Session", &session);
    engine.rootContext()->setContextProperty("AuthViewModel", &authVm);
    engine.rootContext()->setContextProperty("ShellViewModel", &shellVm);
    engine.rootContext()->setContextProperty("SettingsViewModel", &settingsVm);
    engine.rootContext()->setContextProperty("FileDialogService", &fileDialogs);

    const QUrl url(QStringLiteral("qrc:/qt/qml/Xipher/Desktop/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl) {
                             QCoreApplication::exit(-1);
                         }
                     },
                     Qt::QueuedConnection);
    QObject::connect(&sessionManager, &SessionManager::sessionReady, &store,
                     [&store]() { store.bootstrapAfterAuth(); });
    sessionManager.restore();
    engine.load(url);

    return app.exec();
}

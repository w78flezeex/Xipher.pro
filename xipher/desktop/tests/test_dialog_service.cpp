#include <QtTest/QtTest>

#include <QSignalSpy>

#include "services/FileDialogService.h"

class DialogServiceTests : public QObject {
    Q_OBJECT

private slots:
    void returnsPresetFiles();
};

void DialogServiceTests::returnsPresetFiles() {
    qputenv("XIPHER_TEST_DIALOG_FILES", QByteArray("one.txt;two.txt"));

    FileDialogService service;
    QSignalSpy spy(&service, &FileDialogService::filesSelected);
    service.openFiles();

    QVERIFY(spy.wait(100));
    const QList<QVariant> args = spy.takeFirst();
    const QStringList files = args.at(0).toStringList();
    QCOMPARE(files.size(), 2);
    QCOMPARE(files.at(0), QString("one.txt"));
    QCOMPARE(files.at(1), QString("two.txt"));

    qputenv("XIPHER_TEST_DIALOG_FILES", QByteArray());
}

QTEST_MAIN(DialogServiceTests)
#include "test_dialog_service.moc"

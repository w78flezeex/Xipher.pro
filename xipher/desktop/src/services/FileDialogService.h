#pragma once

#include <QObject>
#include <QStringList>

class FileDialogService : public QObject {
    Q_OBJECT

public:
    explicit FileDialogService(QObject* parent = nullptr);

public slots:
    void openFiles();

signals:
    void filesSelected(const QStringList& files);
    void canceled();

private:
    QStringList testFiles() const;
};

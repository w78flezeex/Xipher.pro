#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct UploadItem {
    QString tempId;
    QString filePath;
    QString fileName;
    qint64 fileSize = 0;
    QString state;
    int progress = 0;
    QString errorMessage;
};

class UploadListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TempIdRole = Qt::UserRole + 1,
        FilePathRole,
        FileNameRole,
        FileSizeRole,
        StateRole,
        ProgressRole,
        ErrorRole
    };

    explicit UploadListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addUpload(const UploadItem& item);
    void updateProgress(const QString& tempId, int progress);
    void updateState(const QString& tempId, const QString& state, const QString& errorMessage = QString());
    void removeUpload(const QString& tempId);
    UploadItem uploadById(const QString& tempId) const;

private:
    int indexOf(const QString& tempId) const;

    QVector<UploadItem> uploads_;
};

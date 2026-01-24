#include "store/UploadListModel.h"

UploadListModel::UploadListModel(QObject* parent) : QAbstractListModel(parent) {}

int UploadListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return uploads_.size();
}

QVariant UploadListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= uploads_.size()) {
        return QVariant();
    }
    const UploadItem& item = uploads_.at(index.row());
    switch (role) {
        case TempIdRole:
            return item.tempId;
        case FilePathRole:
            return item.filePath;
        case FileNameRole:
            return item.fileName;
        case FileSizeRole:
            return item.fileSize;
        case StateRole:
            return item.state;
        case ProgressRole:
            return item.progress;
        case ErrorRole:
            return item.errorMessage;
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> UploadListModel::roleNames() const {
    return {
        {TempIdRole, "tempId"},
        {FilePathRole, "filePath"},
        {FileNameRole, "fileName"},
        {FileSizeRole, "fileSize"},
        {StateRole, "state"},
        {ProgressRole, "progress"},
        {ErrorRole, "errorMessage"}
    };
}

void UploadListModel::addUpload(const UploadItem& item) {
    beginInsertRows(QModelIndex(), uploads_.size(), uploads_.size());
    uploads_.push_back(item);
    endInsertRows();
}

void UploadListModel::updateProgress(const QString& tempId, int progress) {
    const int idx = indexOf(tempId);
    if (idx == -1) {
        return;
    }
    UploadItem& item = uploads_[idx];
    item.progress = progress;
    const QModelIndex modelIndex = index(idx);
    emit dataChanged(modelIndex, modelIndex, {ProgressRole});
}

void UploadListModel::updateState(const QString& tempId, const QString& state, const QString& errorMessage) {
    const int idx = indexOf(tempId);
    if (idx == -1) {
        return;
    }
    UploadItem& item = uploads_[idx];
    item.state = state;
    item.errorMessage = errorMessage;
    const QModelIndex modelIndex = index(idx);
    emit dataChanged(modelIndex, modelIndex, {StateRole, ErrorRole});
}

void UploadListModel::removeUpload(const QString& tempId) {
    const int idx = indexOf(tempId);
    if (idx == -1) {
        return;
    }
    beginRemoveRows(QModelIndex(), idx, idx);
    uploads_.removeAt(idx);
    endRemoveRows();
}

UploadItem UploadListModel::uploadById(const QString& tempId) const {
    for (const auto& item : uploads_) {
        if (item.tempId == tempId) {
            return item;
        }
    }
    return UploadItem();
}

int UploadListModel::indexOf(const QString& tempId) const {
    for (int i = 0; i < uploads_.size(); ++i) {
        if (uploads_[i].tempId == tempId) {
            return i;
        }
    }
    return -1;
}

#include "services/FileDialogService.h"

#include <QFileDialog>

FileDialogService::FileDialogService(QObject* parent) : QObject(parent) {}

void FileDialogService::openFiles() {
    const QStringList preset = testFiles();
    if (!preset.isEmpty()) {
        emit filesSelected(preset);
        return;
    }

    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setOption(QFileDialog::ReadOnly, true);
    if (dialog.exec() == QDialog::Accepted) {
        emit filesSelected(dialog.selectedFiles());
    } else {
        emit canceled();
    }
}

QStringList FileDialogService::testFiles() const {
    const QByteArray raw = qgetenv("XIPHER_TEST_DIALOG_FILES");
    if (raw.isEmpty()) {
        return {};
    }
    const QString list = QString::fromUtf8(raw);
    return list.split(';', Qt::SkipEmptyParts);
}

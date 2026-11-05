#include "WorkspaceGuard.h"
#include "model/Errors.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
namespace clinic::persistence {
WorkspaceGuard::WorkspaceGuard(const QString &path) {
    QFileInfo info(path);
    path_ = info.exists() ? info.canonicalFilePath()
                          : QDir(info.absolutePath()).canonicalPath() + "/" + info.fileName();
    if (path_.isEmpty() || !QFileInfo(info.absolutePath()).isDir())
        throw model::FileError("Choose an existing writable folder.");
    lock_ = std::make_unique<QLockFile>(path_ + ".lock");
    lock_->setStaleLockTime(0);
    if (!lock_->tryLock(0))
        throw model::FileError("This workspace is open in another instance or cannot be locked. "
                               "Close the other instance or choose another file.");
    checkpoint();
}
QByteArray WorkspaceGuard::digest() const {
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly))
        throw model::FileError("Cannot verify workspace contents: " + file.errorString());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        throw model::FileError("Cannot read workspace fingerprint.");
    return hash.result();
}
void WorkspaceGuard::checkpoint() {
    existed_ = QFileInfo::exists(path_);
    fingerprint_ = existed_ ? digest() : QByteArray();
}
void WorkspaceGuard::verifyUnchanged() const {
    if (QFileInfo::exists(path_) != existed_ || (existed_ && digest() != fingerprint_))
        throw model::FileError("This workspace changed outside this application. Save As to a new "
                               "file, or reload it; the original was not overwritten.");
}
} // namespace clinic::persistence
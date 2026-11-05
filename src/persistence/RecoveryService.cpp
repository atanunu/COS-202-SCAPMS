#include "RecoveryService.h"
#include "model/Errors.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
namespace clinic::persistence {
QString RecoveryService::recoveryPath(const QString &workspace) {
    if (!workspace.isEmpty())
        return workspace + ".recovery.clinic";
    const auto directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/recovery";
    if (!QDir().mkpath(directory))
        throw model::FileError("Cannot create recovery directory.");
    static const auto session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return directory + "/unsaved-" + session + ".clinic";
}
void RecoveryService::autosave(const model::ClinicDatabase &db, const QString &workspace) {
    FileHandler::save(db, recoveryPath(workspace));
}
void RecoveryService::saveWithBackup(const model::ClinicDatabase &db, const QString &workspace) {
    if (QFileInfo::exists(workspace)) {
        // Validate the existing file before making a protected recovery copy.
        const auto previous = FileHandler::load(workspace);
        const QString folder = workspace + ".backups";
        if (!QDir().mkpath(folder))
            throw model::FileError("Cannot create backup folder.");
        const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        FileHandler::save(previous, folder + "/" + stamp + ".clinic");
    }
    FileHandler::save(db, workspace);
    QDir folder(workspace + ".backups");
    const auto names = folder.entryList({"*.clinic"}, QDir::Files, QDir::Name);
    for (int i = 0; i < names.size() - 5; ++i)
        folder.remove(names[i]);
    discardRecovery(workspace);
}
QStringList RecoveryService::copies(const QString &workspace) {
    QStringList result;
    const auto directory = QFileInfo(recoveryPath({})).absoluteDir();
    for (const auto &name : directory.entryList({"unsaved*.clinic"}, QDir::Files, QDir::Time))
        result.append(directory.filePath(name));
    const auto recovery = recoveryPath(workspace);
    if (QFileInfo::exists(recovery) && !result.contains(recovery))
        result.append(recovery);
    QDir folder(workspace + ".backups");
    for (const auto &name :
         folder.entryList({"*.clinic"}, QDir::Files, QDir::Name | QDir::Reversed))
        result.append(folder.filePath(name));
    return result;
}
void RecoveryService::discardRecovery(const QString &workspace) {
    const auto file = recoveryPath(workspace);
    if (QFileInfo::exists(file) && !QFile::remove(file))
        throw model::FileError("Saved, but the recovery copy could not be removed.");
}
} // namespace clinic::persistence

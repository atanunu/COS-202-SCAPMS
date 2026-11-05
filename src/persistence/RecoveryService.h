#pragma once
#include "FileHandler.h"
namespace clinic::persistence {
class RecoveryService {
  public:
    static QString recoveryPath(const QString &workspace);
    static void autosave(const model::ClinicDatabase &, const QString &workspace);
    static void saveWithBackup(const model::ClinicDatabase &, const QString &workspace);
    static QStringList copies(const QString &workspace);
    static void discardRecovery(const QString &workspace);
};
} // namespace clinic::persistence

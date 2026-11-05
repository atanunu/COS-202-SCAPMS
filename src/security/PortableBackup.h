#pragma once
#include <QByteArray>
#include <QString>
namespace clinic::security {
// Versioned AES-256-GCM backup; independent of DPAPI and the Windows account.
QByteArray encryptBackup(const QByteArray &, const QString &);
QByteArray decryptBackup(const QByteArray &, const QString &);
} // namespace clinic::security
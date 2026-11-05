#include "AccessControl.h"
#include "model/Errors.h"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <dpapi.h>
#endif
namespace clinic::security {
using model::ValidationError;
QString roleName(Role r) {
    switch (r) {
    case Role::Administrator:
        return "Administrator";
    case Role::Clinician:
        return "Clinician";
    case Role::Receptionist:
        return "Receptionist";
    case Role::Viewer:
        return "Viewer";
    }
    throw ValidationError("Invalid staff role.");
}
Role parseRole(const QString &name) {
    for (auto r : {Role::Administrator, Role::Clinician, Role::Receptionist, Role::Viewer})
        if (roleName(r) == name)
            return r;
    throw ValidationError("Unknown staff role.");
}
UserAccount createAccount(const QString &username, const QString &password, Role role) {
    const auto name = username.trimmed().toLower();
    if (!QRegularExpression("^[a-z0-9._-]{3,40}$").match(name).hasMatch())
        throw ValidationError(
            "Username must be 3-40 letters, numbers, dots, hyphens or underscores.");
    if (password.size() < 12 || password.size() > 256)
        throw ValidationError("Use a password or passphrase of 12-256 characters.");
    UserAccount account;
    account.username = name;
    account.role = role;
    account.salt.resize(16);
    for (int i = 0; i < 16; ++i)
        account.salt[i] = static_cast<char>(QRandomGenerator::system()->generate() & 255);
    account.passwordHash = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), account.salt, account.iterations, 32);
    if (account.passwordHash.size() != 32)
        throw ValidationError("Password hashing failed.");
    return account;
}
bool verifyPassword(const UserAccount &account, const QString &password) {
    if (!account.active || password.size() > 256 || account.iterations < 100000 ||
        account.iterations > 1000000)
        return false;
    const auto digest = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), account.salt, account.iterations, 32);
    if (digest.size() != account.passwordHash.size())
        return false;
    unsigned char different = 0;
    for (int i = 0; i < digest.size(); ++i)
        different |= static_cast<unsigned char>(digest[i] ^ account.passwordHash[i]);
    return different == 0;
}
QByteArray protect(const QByteArray &plain) {
#ifdef Q_OS_WIN
    DATA_BLOB input{static_cast<DWORD>(plain.size()),
                    reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))},
        output{};
    if (!CryptProtectData(&input, L"SCAPMS protected workspace", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
        throw model::FileError("Windows could not protect this workspace.");
    QByteArray result(reinterpret_cast<char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(plain);
    throw model::FileError("Protected workspaces require Windows account encryption.");
#endif
}
QByteArray unprotect(const QByteArray &encrypted) {
#ifdef Q_OS_WIN
    DATA_BLOB input{static_cast<DWORD>(encrypted.size()),
                    reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.constData()))},
        output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                            &output))
        throw model::FileError("Cannot decrypt this workspace. Use the Windows account and "
                               "computer that saved it, or a valid backup.");
    QByteArray result(reinterpret_cast<char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    return result;
#else
    Q_UNUSED(encrypted);
    throw model::FileError("Protected workspaces require Windows.");
#endif
}
} // namespace clinic::security

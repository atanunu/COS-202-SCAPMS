#include "PortableBackup.h"
#include "model/Errors.h"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif
namespace clinic::security {
namespace {
const QByteArray magic("SCAPMS-BACKUP-1\n");
constexpr int iterations = 600000;
constexpr int maxBytes = 20 * 1024 * 1024;
QByteArray randomBytes(int count) {
    QByteArray out(count, Qt::Uninitialized);
    for (auto &c : out)
        c = static_cast<char>(QRandomGenerator::system()->generate() & 255);
    return out;
}
QByteArray crypt(const QByteArray &input, const QString &password, const QByteArray &salt,
                 QByteArray nonce, QByteArray &tag, bool encrypt) {
#ifdef Q_OS_WIN
    struct Handles {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_KEY_HANDLE key = nullptr;
        QByteArray secret;
        ~Handles() {
            if (key)
                BCryptDestroyKey(key);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            if (!secret.isEmpty())
                SecureZeroMemory(secret.data(), secret.size());
        }
    } handles;
    auto check = [](NTSTATUS status) {
        if (status < 0)
            throw model::FileError("Backup authentication failed or encryption is unavailable. "
                                   "Check the password and backup file.");
    };
    handles.secret = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                                        password.toUtf8(), salt, iterations, 32);
    if (handles.secret.size() != 32)
        throw model::FileError("Backup key derivation failed.");
    check(BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0));
    check(BCryptSetProperty(handles.algorithm, BCRYPT_CHAINING_MODE,
                            reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                            sizeof(BCRYPT_CHAIN_MODE_GCM), 0));
    check(BCryptGenerateSymmetricKey(handles.algorithm, &handles.key, nullptr, 0,
                                     reinterpret_cast<PUCHAR>(handles.secret.data()),
                                     handles.secret.size(), 0));
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    auto aad = magic + salt + nonce;
    info.pbNonce = reinterpret_cast<PUCHAR>(nonce.data());
    info.cbNonce = nonce.size();
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = tag.size();
    info.pbAuthData = reinterpret_cast<PUCHAR>(aad.data());
    info.cbAuthData = aad.size();
    QByteArray out(input.size(), Qt::Uninitialized);
    ULONG written = 0;
    auto operation = encrypt ? BCryptEncrypt : BCryptDecrypt;
    const auto status = operation(
        handles.key, reinterpret_cast<PUCHAR>(const_cast<char *>(input.constData())), input.size(),
        &info, nullptr, 0, reinterpret_cast<PUCHAR>(out.data()), out.size(), &written, 0);
    if (status < 0) {
        SecureZeroMemory(out.data(), out.size());
        check(status);
    }
    out.resize(written);
    return out;
#else
    Q_UNUSED(input);
    Q_UNUSED(password);
    Q_UNUSED(salt);
    Q_UNUSED(nonce);
    Q_UNUSED(tag);
    Q_UNUSED(encrypt);
    throw model::FileError("Portable backup encryption requires the Windows build.");
#endif
}
} // namespace
QByteArray encryptBackup(const QByteArray &plain, const QString &password) {
    if (password.size() < 12 || password.size() > 256)
        throw model::ValidationError("Use a backup passphrase of 12-256 characters.");
    if (plain.isEmpty() || plain.size() > maxBytes)
        throw model::FileError("Backup data must be 1 byte to 20 MB.");
    const auto salt = randomBytes(16), nonce = randomBytes(12);
    QByteArray tag(16, 0);
    const auto cipher = crypt(plain, password, salt, nonce, tag, true);
    return magic + salt + nonce + tag + cipher;
}
QByteArray decryptBackup(const QByteArray &data, const QString &password) {
    const int header = magic.size() + 44;
    if (!data.startsWith(magic) || data.size() <= header || data.size() > maxBytes + header ||
        password.size() < 12 || password.size() > 256)
        throw model::FileError("Invalid backup format or passphrase.");
    const auto salt = data.mid(magic.size(), 16), nonce = data.mid(magic.size() + 16, 12);
    auto tag = data.mid(magic.size() + 28, 16);
    return crypt(data.mid(header), password, salt, nonce, tag, false);
}
} // namespace clinic::security
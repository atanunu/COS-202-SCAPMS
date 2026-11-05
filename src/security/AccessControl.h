#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QString>
namespace clinic::security {
enum class Role { Administrator, Clinician, Receptionist, Viewer };
QString roleName(Role);
Role parseRole(const QString &);
struct UserAccount {
    QString username;
    Role role = Role::Viewer;
    QByteArray salt;
    QByteArray passwordHash;
    int iterations = 210000;
    bool active = true;
};
struct AuditEntry {
    QDateTime time;
    QString actor;
    QString action;
};
UserAccount createAccount(const QString &, const QString &, Role);
bool verifyPassword(const UserAccount &, const QString &);
QByteArray protect(const QByteArray &);
QByteArray unprotect(const QByteArray &);
} // namespace clinic::security

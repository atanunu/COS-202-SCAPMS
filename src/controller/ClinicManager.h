#pragma once
#include "model/ClinicDatabase.h"
#include <functional>
#include <stack>
namespace clinic::controller {
struct UndoSnapshot {
    model::ClinicDatabase state;
    QString description;
};
class ClinicManager {
  public:
    const model::ClinicDatabase &database() const {
        return database_;
    }
    QString suggestedPatientId() const;
    void addPatient(model::PatientCategory, model::PatientDetails);
    void editPatient(const QString &, model::PatientCategory, model::PatientDetails);
    void deletePatient(const QString &); // Archives; never erases history.
    void restorePatient(const QString &);
    QString schedule(const QString &, const QString &, const QDateTime &, int,
                     const QString &clinician = {}, const QString &room = {});
    void reschedule(const QString &, const QDateTime &, int);
    void checkIn(const QString &);
    void cancel(const QString &, const QString &reason = {});
    void noShow(const QString &, const QString &);
    void correctNotes(const QString &, const QString &, const QString &);
    void assignResources(const QString &, const QString &, const QString &);
    void addResource(const QString &, bool room);
    QStringList possibleDuplicates(const model::PatientDetails &,
                                   const QString &exclude = {}) const;
    bool canExportReports() const {
        return canViewClinicalNotes();
    }
    void requireReportExport() const;
    void recordExport(const QString &);
    void complete(const QString &, const QString &);
    void enqueue(const QString &);
    QString callNext();
    void removeFromQueue(const QString &);
    bool canUndo() const {
        return !history_.empty() && canEdit();
    }
    QString undoDescription() const {
        return canUndo() ? history_.top().description : QString();
    }
    void undo();
    void replace(model::ClinicDatabase, const QString &username = {}, const QString &password = {});
    void newDatabase();
    void access(const QString &id) {
        database_.recordAccess(id);
    }
    bool dirty() const {
        return dirty_;
    }
    void markSaved() {
        dirty_ = false;
    }
    void markUnsaved() {
        dirty_ = true;
    }
    bool demoMode() const {
        return database_.users.empty();
    }
    bool authenticated() const {
        return demoMode() || !username_.isEmpty();
    }
    QString username() const {
        return demoMode() ? "Demo operator" : username_;
    }
    security::Role role() const {
        return demoMode() ? security::Role::Administrator : role_;
    }
    bool canEdit() const {
        return authenticated() && role() != security::Role::Viewer;
    }
    bool isAdmin() const {
        return authenticated() && role() == security::Role::Administrator;
    }
    bool canViewClinicalNotes() const {
        return authenticated() && (isAdmin() || role() == security::Role::Clinician);
    }
    void configureAdministrator(const QString &, const QString &);
    void addUser(const QString &, const QString &, security::Role);
    void setUserActive(const QString &, bool);
    void changePassword(const QString &current, const QString &replacement);
    void login(const QString &, const QString &);
    void lock();

  private:
    model::ClinicDatabase database_;
    std::stack<UndoSnapshot> history_;
    bool dirty_ = false;
    QString username_;
    security::Role role_ = security::Role::Viewer;
    int failedLogins_ = 0;
    QDateTime lockedUntil_;
    void transact(const QString &, const std::function<void(model::ClinicDatabase &)> &);
    void requireEdit() const;
    void audit(const QString &);
};
} // namespace clinic::controller

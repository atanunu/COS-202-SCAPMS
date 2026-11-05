#include "ClinicManager.h"
#include "model/Appointable.h"
#include "model/Errors.h"
#include <limits>
namespace clinic::controller {
using namespace model;
namespace {
QString generateId(const QString &prefix, int &next,
                   const std::function<bool(const QString &)> &exists) {
    while (next < std::numeric_limits<int>::max()) {
        const QString value = prefix + QString("%1").arg(next++, 4, 10, QChar('0'));
        if (!exists(value))
            return value;
    }
    throw ValidationError("ID space exhausted.");
}
const PatientRecord &requirePatient(const ClinicDatabase &db, const QString &id,
                                    bool allowArchived = false) {
    const auto *patient = db.patient(id);
    if (!patient)
        throw ValidationError("Select an existing patient.");
    if (!allowArchived && db.archivedPatients.contains(id))
        throw ValidationError(
            "Restore this archived patient before changing their record or booking a visit.");
    return *patient;
}
Appointment &requireAppointment(ClinicDatabase &db, const QString &id) {
    auto *appointment = db.appointment(id);
    if (!appointment)
        throw ValidationError("Select an existing appointment.");
    return *appointment;
}
void removeQueued(ClinicDatabase &db, const QString &id) {
    std::queue<QueueEntry> updated;
    while (!db.waiting.empty()) {
        if (db.waiting.front().patientId != id)
            updated.push(db.waiting.front());
        db.waiting.pop();
    }
    db.waiting = std::move(updated);
}
} // namespace
void ClinicManager::requireEdit() const {
    if (!canEdit())
        throw ValidationError("Your current role cannot modify clinic records.");
}
void ClinicManager::audit(const QString &action) {
    database_.audit.push_back({QDateTime::currentDateTimeUtc(), username(), action});
    dirty_ = true;
}
void ClinicManager::transact(const QString &description,
                             const std::function<void(ClinicDatabase &)> &operation) {
    requireEdit();
    ClinicDatabase candidate(database_);
    operation(candidate);
    candidate.validate();
    // Bound memory while retaining the explicitly required stack container.
    if (history_.size() >= 50) {
        std::vector<UndoSnapshot> newest;
        while (!history_.empty()) {
            newest.push_back(std::move(history_.top()));
            history_.pop();
        }
        newest.pop_back();
        for (auto it = newest.rbegin(); it != newest.rend(); ++it)
            history_.push(std::move(*it));
    }
    auto snapshot = database_;
    snapshot.audit.clear(); // Audit is append-only across undo.
    history_.push({std::move(snapshot), description});
    database_ = std::move(candidate);
    audit(description);
}
QString ClinicManager::suggestedPatientId() const {
    int next = database_.nextPatientId;
    return generateId("P", next,
                      [this](const QString &id) { return database_.patient(id) != nullptr; });
}
void ClinicManager::addPatient(PatientCategory category, PatientDetails details) {
    if (!canViewClinicalNotes()) {
        category = PatientCategory::General;
        details.reason = "Awaiting clinician assessment";
        details.urgency = 1;
        details.previousVisit.clear();
    }
    transact("Register patient " + details.name.trimmed(), [&](ClinicDatabase &db) {
        if (details.id.trimmed().isEmpty())
            details.id = generateId("P", db.nextPatientId,
                                    [&](const QString &id) { return db.patient(id) != nullptr; });
        db.patients.push_back(makePatient(category, details));
    });
}
void ClinicManager::editPatient(const QString &id, PatientCategory category,
                                PatientDetails details) {
    transact("Edit patient " + id, [&](ClinicDatabase &db) {
        const auto &old = requirePatient(db, id);
        if (!canViewClinicalNotes()) {
            category = old.category();
            details.reason = old.details().reason;
            details.urgency = old.details().urgency;
            details.previousVisit = old.details().previousVisit;
        }
        details.id = id;
        for (auto &patient : db.patients)
            if (patient->id() == id) {
                patient = makePatient(category, details);
                break;
            }
    });
}
void ClinicManager::deletePatient(const QString &id) {
    if (!isAdmin())
        throw ValidationError("Only an administrator can archive a patient.");
    transact("Archive patient " + id, [&](ClinicDatabase &db) {
        requirePatient(db, id);
        for (const auto &a : db.appointments)
            if (a.patientId == id && a.status == AppointmentStatus::InConsultation)
                throw ValidationError(
                    "Finish the active consultation before archiving this patient.");
        db.archivedPatients.insert(id);
        for (auto &a : db.appointments)
            if (a.patientId == id && isPending(a.status)) {
                a.status = AppointmentStatus::Cancelled;
                a.changeReason = "Patient archived";
            }
        removeQueued(db, id);
        for (auto &e : db.frequent)
            if (e.patientId == id)
                e = {};
    });
}
void ClinicManager::restorePatient(const QString &id) {
    if (!isAdmin())
        throw ValidationError("Only an administrator can restore archived patients.");
    transact("Restore patient " + id, [&](ClinicDatabase &db) {
        requirePatient(db, id, true);
        if (!db.archivedPatients.remove(id))
            throw ValidationError("This patient is not archived.");
    });
}
QString ClinicManager::schedule(const QString &patientId, const QString &service,
                                const QDateTime &start, int minutes, const QString &clinician,
                                const QString &room) {
    QString result;
    transact("Schedule visit for " + patientId, [&](ClinicDatabase &db) {
        const auto &patient = requirePatient(db, patientId);
        if (!start.isValid() || start < QDateTime::currentDateTime())
            throw ValidationError("New appointments must be scheduled in the future.");
        Appointment a;
        a.id = generateId("A", db.nextAppointmentId,
                          [&](const QString &id) { return db.appointment(id) != nullptr; });
        a.patientId = patientId;
        a.service = service.trimmed();
        a.start = start;
        a.durationMinutes = minutes;
        a.clinician = clinician;
        a.room = room;
        const auto *policy = dynamic_cast<const Appointable *>(&patient);
        if (!policy)
            throw ValidationError("This patient cannot be scheduled.");
        policy->validateScheduling(a);
        result = a.id;
        db.appointments.push_back(a);
    });
    return result;
}
void ClinicManager::reschedule(const QString &id, const QDateTime &start, int minutes) {
    transact("Reschedule appointment " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        requirePatient(db, a.patientId);
        if (a.status != AppointmentStatus::Scheduled)
            throw ValidationError(
                "Only scheduled, not-yet-checked-in appointments can be rescheduled.");
        if (!start.isValid() || start < QDateTime::currentDateTime())
            throw ValidationError("Choose a future appointment time.");
        a.start = start;
        a.durationMinutes = minutes;
    });
}
void ClinicManager::checkIn(const QString &id) {
    transact("Check in appointment " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        requirePatient(db, a.patientId);
        if (a.status != AppointmentStatus::Scheduled)
            throw ValidationError("Only a scheduled appointment can be checked in.");
        if (a.start.toLocalTime().date() != QDate::currentDate())
            throw ValidationError("Check in is available on the appointment date.");
        a.status = AppointmentStatus::CheckedIn;
        db.waiting.push({a.patientId, QDateTime::currentDateTime(), a.id});
    });
}
void ClinicManager::cancel(const QString &id, const QString &reason) {
    if (reason.trimmed().isEmpty() || reason.size() > 1000)
        throw ValidationError("Enter a cancellation reason (1-1000 characters).");
    transact("Cancel appointment " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        if (a.status != AppointmentStatus::Scheduled && a.status != AppointmentStatus::CheckedIn)
            throw ValidationError("Only scheduled or checked-in visits can be cancelled.");
        if (a.status == AppointmentStatus::Scheduled) {
            auto *policy = dynamic_cast<const Appointable *>(&requirePatient(db, a.patientId));
            if (policy)
                policy->validateCancellation(a);
        }
        if (a.status == AppointmentStatus::CheckedIn)
            removeQueued(db, a.patientId);
        a.status = AppointmentStatus::Cancelled;
        a.changeReason = reason.trimmed();
    });
}
void ClinicManager::complete(const QString &id, const QString &notes) {
    if (!canViewClinicalNotes())
        throw ValidationError("Only clinicians and administrators can complete consultations.");
    transact("Complete consultation " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        if (a.status != AppointmentStatus::InConsultation)
            throw ValidationError(
                "Call this patient from the queue to start consultation before completing it.");
        if (a.consultationStarted.isValid() && a.consultationStarted > QDateTime::currentDateTime())
            throw ValidationError("Consultation start time is in the future.");
        if (notes.trimmed().isEmpty() || notes.size() > 10000)
            throw ValidationError("Enter consultation notes (1-10000 characters).");
        a.consultationNotes = notes.trimmed();
        a.completedAt = QDateTime::currentDateTime();
        a.status = AppointmentStatus::Completed;
        removeQueued(db, a.patientId);
    });
}
void ClinicManager::enqueue(const QString &patientId) {
    transact("Add patient " + patientId + " to waiting queue", [&](ClinicDatabase &db) {
        requirePatient(db, patientId);
        for (const auto &a : db.appointments)
            if (a.patientId == patientId && a.status == AppointmentStatus::InConsultation)
                throw ValidationError("This patient is already in consultation.");
        Appointment a;
        a.id = generateId("A", db.nextAppointmentId,
                          [&](const QString &v) { return db.appointment(v); });
        a.patientId = patientId;
        a.service = "Walk-in consultation";
        a.start = QDateTime::currentDateTime();
        a.status = AppointmentStatus::CheckedIn;
        a.walkIn = true;
        db.appointments.push_back(a);
        db.waiting.push({patientId, a.start, a.id});
    });
}
QString ClinicManager::callNext() {
    if (!canViewClinicalNotes())
        throw ValidationError("Only clinicians and administrators can call the next patient.");
    QString summary;
    transact("Call next patient", [&](ClinicDatabase &db) {
        if (db.waiting.empty())
            throw ValidationError("The waiting queue is empty.");
        auto entry = db.waiting.front();
        const auto &patient = requirePatient(db, entry.patientId);
        if (entry.appointmentId.isEmpty()) {
            Appointment a;
            a.id = generateId("A", db.nextAppointmentId,
                              [&](const QString &v) { return db.appointment(v); });
            a.patientId = entry.patientId;
            a.start = QDateTime::currentDateTime();
            a.service = "Walk-in consultation";
            a.walkIn = true;
            db.appointments.push_back(a);
            entry.appointmentId = a.id;
        }
        auto &visit = requireAppointment(db, entry.appointmentId);
        visit.status = AppointmentStatus::InConsultation;
        visit.consultationStarted = QDateTime::currentDateTime();
        summary = patient.details().name + " (" + patient.id() + ")\n" + processPatient(patient);
        db.waiting.pop();
    });
    return summary;
}
void ClinicManager::removeFromQueue(const QString &patientId) {
    transact("Remove " + patientId + " from waiting queue", [&](ClinicDatabase &db) {
        auto queue = db.waiting;
        bool found = false;
        while (!queue.empty()) {
            const auto entry = queue.front();
            if (entry.patientId == patientId) {
                found = true;
                if (!entry.appointmentId.isEmpty())
                    requireAppointment(db, entry.appointmentId).status =
                        AppointmentStatus::Scheduled;
            }
            queue.pop();
        }
        if (!found)
            throw ValidationError("Select a waiting patient.");
        removeQueued(db, patientId);
    });
}
void ClinicManager::undo() {
    requireEdit();
    if (!canUndo())
        throw ValidationError("There is no operation to undo.");
    const auto description = history_.top().description;
    auto auditTrail = std::move(database_.audit);
    database_ = std::move(history_.top().state);
    history_.pop();
    database_.audit = std::move(auditTrail);
    audit("Undo: " + description);
}
void ClinicManager::replace(ClinicDatabase database, const QString &username,
                            const QString &password) {
    database.validate();
    QString accepted;
    auto role = security::Role::Viewer;
    if (!database.users.empty()) {
        for (const auto &account : database.users)
            if (account.username == username.trimmed().toLower() &&
                security::verifyPassword(account, password)) {
                accepted = account.username;
                role = account.role;
                break;
            }
        if (accepted.isEmpty())
            throw ValidationError("Invalid username or password.");
    }
    database_ = std::move(database);
    username_ = accepted;
    role_ = role;
    history_ = {};
    dirty_ = false;
    failedLogins_ = 0;
}
void ClinicManager::newDatabase() {
    replace(ClinicDatabase{});
}
void ClinicManager::configureAdministrator(const QString &username, const QString &password) {
    if (!demoMode())
        throw ValidationError("Access control is already configured.");
    auto account = security::createAccount(username, password, security::Role::Administrator);
    database_.users.push_back(account);
    username_ = account.username;
    role_ = account.role;
    history_ = {};
    audit("Enable staff access control");
}
void ClinicManager::addUser(const QString &username, const QString &password, security::Role role) {
    if (!isAdmin())
        throw ValidationError("Administrator access is required.");
    if (demoMode())
        throw ValidationError("Create the initial administrator first.");
    auto account = security::createAccount(username, password, role);
    for (const auto &user : database_.users)
        if (user.username == account.username)
            throw ValidationError("Username already exists.");
    database_.users.push_back(account);
    history_ = {};
    audit("Add staff account " + account.username + " (" + security::roleName(role) + ")");
}
void ClinicManager::setUserActive(const QString &username, bool active) {
    if (!isAdmin())
        throw ValidationError("Administrator access is required.");
    if (username == username_)
        throw ValidationError("You cannot disable your current account.");
    for (auto &user : database_.users)
        if (user.username == username) {
            user.active = active;
            history_ = {};
            audit(QString(active ? "Enable staff account " : "Disable staff account ") + username);
            return;
        }
    throw ValidationError("Staff account not found.");
}
void ClinicManager::changePassword(const QString &current, const QString &replacement) {
    if (!authenticated() || demoMode())
        throw ValidationError("Sign in to a staff account first.");
    for (auto &user : database_.users)
        if (user.username == username_) {
            if (!security::verifyPassword(user, current))
                throw ValidationError("Current password is incorrect.");
            user = security::createAccount(user.username, replacement, user.role);
            history_ = {};
            audit("Change own password");
            return;
        }
}
void ClinicManager::login(const QString &name, const QString &password) {
    if (lockedUntil_ > QDateTime::currentDateTime())
        throw ValidationError("Too many sign-in attempts. Wait 30 seconds.");
    for (const auto &account : database_.users)
        if (account.username == name.trimmed().toLower() &&
            security::verifyPassword(account, password)) {
            username_ = account.username;
            role_ = account.role;
            failedLogins_ = 0;
            history_ = {};
            audit("Staff sign in");
            return;
        }
    if (++failedLogins_ >= 5) {
        lockedUntil_ = QDateTime::currentDateTime().addSecs(30);
        failedLogins_ = 0;
    }
    audit("Failed sign-in attempt");
    throw ValidationError("Invalid username or password.");
}
void ClinicManager::lock() {
    if (demoMode())
        return;
    audit("Lock workspace");
    username_.clear();
    role_ = security::Role::Viewer;
    history_ = {};
}

void ClinicManager::requireReportExport() const {
    if (!canExportReports())
        throw ValidationError("Only clinicians and administrators may export or print reports.");
}
void ClinicManager::recordExport(const QString &what) {
    requireReportExport();
    audit(what);
}
QStringList ClinicManager::possibleDuplicates(const PatientDetails &d,
                                              const QString &exclude) const {
    QStringList matches;
    for (const auto &p : database_.patients) {
        if (p->id() == exclude)
            continue;
        const auto &x = p->details();
        if ((x.name.simplified().compare(d.name.simplified(), Qt::CaseInsensitive) == 0 &&
             x.age == d.age) ||
            x.contact.trimmed().compare(d.contact.trimmed(), Qt::CaseInsensitive) == 0)
            matches << x.id + " / " + x.name +
                           (database_.archivedPatients.contains(x.id) ? " (archived)" : "");
    }
    return matches;
}
void ClinicManager::noShow(const QString &id, const QString &reason) {
    transact("Mark no-show " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        if (a.status != AppointmentStatus::Scheduled || a.end() > QDateTime::currentDateTime())
            throw ValidationError(
                "Only scheduled visits whose end time has passed can be marked no-show.");
        if (reason.trimmed().isEmpty() || reason.size() > 1000)
            throw ValidationError("Enter a no-show reason (1-1000 characters).");
        a.status = AppointmentStatus::NoShow;
        a.changeReason = reason.trimmed();
    });
}
void ClinicManager::correctNotes(const QString &id, const QString &notes, const QString &reason) {
    if (!canViewClinicalNotes())
        throw ValidationError("Clinical access required.");
    transact("Correct consultation notes " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        if (a.status != AppointmentStatus::Completed)
            throw ValidationError("Select a completed consultation.");
        if (notes.trimmed().isEmpty() || notes.size() > 10000 || reason.trimmed().isEmpty() ||
            reason.size() > 1000)
            throw ValidationError(
                "Enter notes (1-10000 characters) and a correction reason (1-1000 characters).");
        a.corrections << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " / " +
                             username() + " / " + reason.trimmed() +
                             "\nPrevious notes: " + a.consultationNotes;
        a.consultationNotes = notes.trimmed();
        a.changeReason = reason.trimmed();
    });
}
void ClinicManager::assignResources(const QString &id, const QString &clinician,
                                    const QString &room) {
    transact("Allocate resources " + id, [&](ClinicDatabase &db) {
        auto &a = requireAppointment(db, id);
        if (!isPending(a.status))
            throw ValidationError("Only active visits can be allocated.");
        a.clinician = clinician;
        a.room = room;
    });
}
void ClinicManager::addResource(const QString &name, bool room) {
    if (!isAdmin())
        throw ValidationError("Only administrators can manage resources.");
    transact("Add " + QString(room ? "room " : "clinician ") + name,
             [&](ClinicDatabase &db) { (room ? db.rooms : db.clinicians).append(name.trimmed()); });
}

} // namespace clinic::controller

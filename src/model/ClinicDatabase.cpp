#include "ClinicDatabase.h"
#include "Errors.h"
#include "algorithms/Algorithms.h"
#include <QSet>
#include <limits>

namespace clinic::model {
ClinicDatabase::ClinicDatabase(const ClinicDatabase &other)
    : appointments(other.appointments), waiting(other.waiting), frequent(other.frequent),
      archivedPatients(other.archivedPatients), clinicians(other.clinicians), rooms(other.rooms),
      users(other.users), audit(other.audit), accessClock(other.accessClock),
      nextPatientId(other.nextPatientId), nextAppointmentId(other.nextAppointmentId) {
    for (const auto &patient : other.patients)
        patients.push_back(patient->clone());
}
ClinicDatabase &ClinicDatabase::operator=(const ClinicDatabase &other) {
    if (this != &other) {
        ClinicDatabase copy(other);
        *this = std::move(copy);
    }
    return *this;
}
const PatientRecord *ClinicDatabase::patient(const QString &id) const {
    return algorithms::linearSearchById(patients, id);
}
Appointment *ClinicDatabase::appointment(const QString &id) {
    for (auto &appointment : appointments)
        if (appointment.id == id)
            return &appointment;
    return nullptr;
}
const Appointment *ClinicDatabase::appointment(const QString &id) const {
    for (const auto &appointment : appointments)
        if (appointment.id == id)
            return &appointment;
    return nullptr;
}
void ClinicDatabase::recordAccess(const QString &id) {
    if (!patient(id) || archivedPatients.contains(id))
        return;
    ++accessClock;
    for (auto &entry : frequent)
        if (entry.patientId == id) {
            if (entry.accesses < std::numeric_limits<int>::max())
                ++entry.accesses;
            entry.lastAccess = accessClock;
            return;
        }
    size_t victim = 0;
    for (size_t i = 0; i < frequent.size(); ++i) {
        if (frequent[i].patientId.isEmpty()) {
            victim = i;
            break;
        }
        if (frequent[i].accesses < frequent[victim].accesses ||
            (frequent[i].accesses == frequent[victim].accesses &&
             frequent[i].lastAccess < frequent[victim].lastAccess))
            victim = i;
    }
    frequent[victim] = {id, 1, accessClock};
}
void ClinicDatabase::validate() const {
    QSet<QString> ids;
    for (const auto &p : patients) {
        if (!p)
            throw ValidationError("Null patient record.");
        if (ids.contains(p->id()))
            throw ValidationError("Patient ID already exists: " + p->id());
        ids.insert(p->id());
    }
    for (const auto &catalog : {clinicians, rooms}) {
        QSet<QString> names;
        for (const auto &name : catalog) {
            if (name.trimmed().isEmpty() || name.size() > 80 || names.contains(name.toCaseFolded()))
                throw ValidationError("Resource names must be unique and 1-80 characters.");
            names.insert(name.toCaseFolded());
        }
    }
    QSet<QString> appointmentIds;
    for (const auto &a : appointments) {
        if (a.id.trimmed().isEmpty() || appointmentIds.contains(a.id))
            throw ValidationError("Missing or duplicate appointment ID.");
        appointmentIds.insert(a.id);
        if ((!a.clinician.isEmpty() && !clinicians.contains(a.clinician)) ||
            (!a.room.isEmpty() && !rooms.contains(a.room)))
            throw ValidationError("Appointment refers to an unknown clinician or room.");
        if (!ids.contains(a.patientId))
            throw ValidationError("Appointment refers to a missing patient: " + a.patientId);
        if (!a.start.isValid() || a.durationMinutes < 5 || a.durationMinutes > 240 ||
            a.service.trimmed().isEmpty())
            throw ValidationError(
                "Appointment requires a valid time, service and duration of 5-240 minutes.");
        if (a.status != AppointmentStatus::Scheduled && a.status != AppointmentStatus::Completed &&
            a.status != AppointmentStatus::Cancelled && a.status != AppointmentStatus::CheckedIn &&
            a.status != AppointmentStatus::InConsultation && a.status != AppointmentStatus::NoShow)
            throw ValidationError("Invalid appointment status.");
    }
    for (size_t i = 0; i < appointments.size(); ++i) {
        const auto &a = appointments[i];
        if (!isPending(a.status))
            continue;
        for (size_t j = i + 1; j < appointments.size(); ++j) {
            const auto &b = appointments[j];
            if (isPending(b.status) && a.start < b.end() && b.start < a.end()) {
                if (a.patientId == b.patientId)
                    throw ValidationError("This patient has overlapping scheduled appointments.");
                if (!a.clinician.isEmpty() && a.clinician == b.clinician)
                    throw ValidationError("The clinician is already booked at this time.");
                if (!a.room.isEmpty() && a.room == b.room)
                    throw ValidationError("The room is already booked at this time.");
            }
        }
    }
    QSet<QString> queued;
    auto copy = waiting;
    while (!copy.empty()) {
        const auto &e = copy.front();
        if (!ids.contains(e.patientId) || archivedPatients.contains(e.patientId) ||
            queued.contains(e.patientId) || !e.arrivedAt.isValid())
            throw ValidationError("Invalid or duplicate patient in the waiting queue.");
        if (!e.appointmentId.isEmpty()) {
            const auto *linked = appointment(e.appointmentId);
            if (!linked || linked->patientId != e.patientId ||
                linked->status != AppointmentStatus::CheckedIn)
                throw ValidationError("Invalid queue appointment link.");
        }
        queued.insert(e.patientId);
        copy.pop();
    }
    for (const auto &id : archivedPatients)
        if (!ids.contains(id))
            throw ValidationError("Archived patient is missing.");
    for (const auto &a : appointments) {
        if (archivedPatients.contains(a.patientId) && isPending(a.status))
            throw ValidationError("Archived patients cannot have active appointments.");
        if (a.status == AppointmentStatus::CheckedIn) {
            auto q = waiting;
            bool found = false;
            while (!q.empty()) {
                if (q.front().appointmentId == a.id)
                    found = true;
                q.pop();
            }
            if (!found)
                throw ValidationError("Checked-in appointment is missing from queue.");
        }
    }
    QSet<QString> inConsultation;
    for (const auto &a : appointments) {
        if (a.status == AppointmentStatus::InConsultation) {
            if (inConsultation.contains(a.patientId) || queued.contains(a.patientId))
                throw ValidationError(
                    "A patient cannot be waiting and in consultation simultaneously.");
            inConsultation.insert(a.patientId);
        }
    }
    QSet<QString> names;
    bool admin = users.empty();
    for (const auto &u : users) {
        if (u.username.isEmpty() || names.contains(u.username) || u.salt.size() != 16 ||
            u.passwordHash.size() != 32 || u.iterations < 100000 || u.iterations > 1000000)
            throw ValidationError("Invalid staff account.");
        names.insert(u.username);
        if (u.active && u.role == security::Role::Administrator)
            admin = true;
    }
    if (!admin)
        throw ValidationError("An active administrator is required.");
    if (nextPatientId < 1 || nextAppointmentId < 1)
        throw ValidationError("Invalid ID generator state.");
}
} // namespace clinic::model

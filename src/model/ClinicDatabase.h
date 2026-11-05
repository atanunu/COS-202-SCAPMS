#pragma once
#include "Appointment.h"
#include "PatientRecord.h"
#include "security/AccessControl.h"
#include <QSet>
#include <array>
#include <queue>
#include <vector>

namespace clinic::model {
struct CacheEntry {
    QString patientId;
    int accesses = 0;
    quint64 lastAccess = 0;
};
class ClinicDatabase {
  public:
    std::vector<std::unique_ptr<PatientRecord>> patients;
    std::vector<Appointment> appointments;
    std::queue<QueueEntry> waiting;
    std::array<CacheEntry, 10> frequent{};
    QSet<QString> archivedPatients;
    QStringList clinicians, rooms;
    std::vector<security::UserAccount> users;
    std::vector<security::AuditEntry> audit;
    quint64 accessClock = 0;
    int nextPatientId = 1;
    int nextAppointmentId = 1;
    ClinicDatabase() = default;
    ClinicDatabase(const ClinicDatabase &other);
    ClinicDatabase &operator=(const ClinicDatabase &other);
    ClinicDatabase(ClinicDatabase &&) noexcept = default;
    ClinicDatabase &operator=(ClinicDatabase &&) noexcept = default;
    const PatientRecord *patient(const QString &id) const;
    Appointment *appointment(const QString &id);
    const Appointment *appointment(const QString &id) const;
    void recordAccess(const QString &id);
    void validate() const;
};
} // namespace clinic::model

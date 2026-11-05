#include "EmergencyPatient.h"
#include "Errors.h"
#include "FollowUpPatient.h"
#include "GeneralPatient.h"

namespace clinic::model {
namespace {
void schedulingPolicy(const PatientRecord &patient, const Appointment &appointment) {
    if (appointment.patientId != patient.id())
        throw ValidationError("Appointment patient does not match.");
    if (!appointment.start.isValid())
        throw ValidationError("Choose a valid appointment date and time.");
}
void cancellationPolicy(const Appointment &appointment) {
    if (appointment.status != AppointmentStatus::Scheduled)
        throw ValidationError("Only scheduled appointments can be cancelled.");
}
} // namespace
GeneralPatient::GeneralPatient(PatientDetails details) : PatientRecord(std::move(details)) {}
QString GeneralPatient::consultationSummary() const {
    return "General consultation: " + details_.reason;
}
std::unique_ptr<PatientRecord> GeneralPatient::clone() const {
    return std::make_unique<GeneralPatient>(*this);
}
void GeneralPatient::validateScheduling(const Appointment &a) const {
    schedulingPolicy(*this, a);
}
void GeneralPatient::validateCancellation(const Appointment &a) const {
    cancellationPolicy(a);
}

EmergencyPatient::EmergencyPatient(PatientDetails details) : PatientRecord(std::move(details)) {
    if (details_.urgency < 1 || details_.urgency > 5)
        throw ValidationError("Urgency must be between 1 and 5.");
}
QString EmergencyPatient::consultationSummary() const {
    return QString("Urgent review (level %1/5): %2").arg(details_.urgency).arg(details_.reason);
}
std::unique_ptr<PatientRecord> EmergencyPatient::clone() const {
    return std::make_unique<EmergencyPatient>(*this);
}
void EmergencyPatient::validateScheduling(const Appointment &a) const {
    schedulingPolicy(*this, a);
}
void EmergencyPatient::validateCancellation(const Appointment &a) const {
    cancellationPolicy(a);
}

FollowUpPatient::FollowUpPatient(PatientDetails details) : PatientRecord(std::move(details)) {
    if (details_.previousVisit.isEmpty() || details_.previousVisit.size() > 120)
        throw ValidationError(
            "Follow-up patients require a previous visit reference (up to 120 characters).");
}
QString FollowUpPatient::consultationSummary() const {
    return "Review of " + details_.previousVisit + ": " + details_.reason;
}
std::unique_ptr<PatientRecord> FollowUpPatient::clone() const {
    return std::make_unique<FollowUpPatient>(*this);
}
void FollowUpPatient::validateScheduling(const Appointment &a) const {
    schedulingPolicy(*this, a);
}
void FollowUpPatient::validateCancellation(const Appointment &a) const {
    cancellationPolicy(a);
}
} // namespace clinic::model

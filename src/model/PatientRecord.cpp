#include "PatientRecord.h"
#include "EmergencyPatient.h"
#include "Errors.h"
#include "FollowUpPatient.h"
#include "GeneralPatient.h"
#include <QRegularExpression>

namespace clinic::model {
QString categoryName(PatientCategory category) {
    switch (category) {
    case PatientCategory::General:
        return "General";
    case PatientCategory::Emergency:
        return "Emergency";
    case PatientCategory::FollowUp:
        return "Follow-up";
    }
    throw ValidationError("Unknown patient category.");
}
PatientCategory parseCategory(const QString &value) {
    for (auto category :
         {PatientCategory::General, PatientCategory::Emergency, PatientCategory::FollowUp})
        if (value == categoryName(category))
            return category;
    throw ValidationError("Unknown patient category: " + value);
}
PatientRecord::PatientRecord(PatientDetails details) : details_(std::move(details)) {
    details_.id = details_.id.trimmed();
    details_.name = details_.name.trimmed();
    details_.contact = details_.contact.trimmed();
    details_.reason = details_.reason.trimmed();
    details_.previousVisit = details_.previousVisit.trimmed();
    static const QRegularExpression idPattern("^[A-Za-z0-9_-]{1,40}$");
    if (!idPattern.match(details_.id).hasMatch())
        throw ValidationError(
            "Patient ID must contain 1-40 letters, numbers, hyphens or underscores.");
    if (details_.name.isEmpty() || details_.name.size() > 120)
        throw ValidationError("Patient name must contain 1-120 characters.");
    if (details_.age < 0 || details_.age > 120)
        throw ValidationError("Patient age must be between 0 and 120.");
    if (details_.contact.isEmpty() || details_.contact.size() > 160)
        throw ValidationError("Enter contact information (up to 160 characters).");
    const QRegularExpression email(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");
    const QRegularExpression phone(R"(^\+?[0-9 ()-]+$)");
    QString digits = details_.contact;
    digits.remove(QRegularExpression("[^0-9]"));
    if (!email.match(details_.contact).hasMatch() &&
        !(phone.match(details_.contact).hasMatch() && digits.size() >= 7 && digits.size() <= 15))
        throw ValidationError("Enter a valid email address or phone number with 7-15 digits.");
    if (details_.reason.isEmpty() || details_.reason.size() > 1000)
        throw ValidationError("Enter a reason for the visit (up to 1000 characters).");
}
std::unique_ptr<PatientRecord> makePatient(PatientCategory category, PatientDetails details) {
    switch (category) {
    case PatientCategory::General:
        return std::make_unique<GeneralPatient>(std::move(details));
    case PatientCategory::Emergency:
        return std::make_unique<EmergencyPatient>(std::move(details));
    case PatientCategory::FollowUp:
        return std::make_unique<FollowUpPatient>(std::move(details));
    }
    throw ValidationError("Unknown patient category.");
}
QString processPatient(const PatientRecord &patient) {
    return patient.consultationSummary();
}
} // namespace clinic::model

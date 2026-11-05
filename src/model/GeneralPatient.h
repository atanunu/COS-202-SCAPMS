#pragma once
#include "Appointable.h"
#include "PatientRecord.h"
namespace clinic::model {
class GeneralPatient final : public PatientRecord, public Appointable {
  public:
    explicit GeneralPatient(PatientDetails details);
    PatientCategory category() const override {
        return PatientCategory::General;
    }
    QString consultationSummary() const override;
    std::unique_ptr<PatientRecord> clone() const override;
    void validateScheduling(const Appointment &) const override;
    void validateCancellation(const Appointment &) const override;
};
} // namespace clinic::model

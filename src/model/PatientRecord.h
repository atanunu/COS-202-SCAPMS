#pragma once
#include <QString>
#include <memory>

namespace clinic::model {
enum class PatientCategory { General, Emergency, FollowUp };
QString categoryName(PatientCategory category);
PatientCategory parseCategory(const QString &value);
struct PatientDetails {
    QString id;
    QString name;
    int age = 0;
    QString contact;
    QString reason;
    int urgency = 1;
    QString previousVisit;
};
class PatientRecord {
  public:
    explicit PatientRecord(PatientDetails details);
    virtual ~PatientRecord() = default;
    const PatientDetails &details() const {
        return details_;
    }
    const QString &id() const {
        return details_.id;
    }
    virtual PatientCategory category() const = 0;
    virtual QString consultationSummary() const = 0;
    virtual std::unique_ptr<PatientRecord> clone() const = 0;

  protected:
    PatientDetails details_;
};
std::unique_ptr<PatientRecord> makePatient(PatientCategory category, PatientDetails details);
QString processPatient(const PatientRecord &patient);
} // namespace clinic::model

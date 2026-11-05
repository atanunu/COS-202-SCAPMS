#pragma once
#include "model/PatientRecord.h"
#include "model/ClinicDatabase.h"
#include <QDialog>
class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;
namespace clinic::gui {
class PatientDialog final : public QDialog {
  public:
    explicit PatientDialog(const QString &suggestedId,
                           const model::PatientRecord *patient = nullptr,
                           QWidget *parent = nullptr);
    model::PatientDetails details() const;
    model::PatientCategory category() const;
    void configureAccess(bool, const model::ClinicDatabase &);

  private:
    QLineEdit *id_, *name_, *contact_, *reason_, *previous_;
    QSpinBox *age_, *urgency_;
    QComboBox *category_;
    QComboBox *previousChoices_ = nullptr;
    bool clinicalAccess_ = true;
    QLabel *urgencyLabel_, *previousLabel_, *error_;
    void updateFields();
};
} // namespace clinic::gui

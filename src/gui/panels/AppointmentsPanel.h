#pragma once
#include "PanelBase.h"
namespace clinic::gui {
class AppointmentsPanel final : public PanelBase {
  public:
    AppointmentsPanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void refresh() override;

  private:
    QComboBox *patient_, *service_, *status_, *clinician_, *room_;
    QDateTimeEdit *time_;
    QSpinBox *duration_;
    QCheckBox *today_;
    QTableWidget *table_;
    void reschedule();
};
} // namespace clinic::gui

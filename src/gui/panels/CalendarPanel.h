#pragma once
#include "PanelBase.h"
namespace clinic::gui {
class CalendarPanel final : public PanelBase {
  public:
    CalendarPanel(controller::ClinicManager &, MutationHandler, QWidget * = nullptr);
    void refresh() override;

  private:
    QCalendarWidget *calendar_;
    QComboBox *clinician_, *room_;
    QTableWidget *table_;
    QLabel *summary_;
    void allocate();
};
} // namespace clinic::gui
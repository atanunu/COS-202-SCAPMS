#pragma once
#include "PanelBase.h"
namespace clinic::gui {
class DashboardPanel final : public PanelBase {
  public:
    DashboardPanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void refresh() override;

  private:
    QLabel *patients_, *pending_, *waiting_, *completed_, *reminder_;
    QTableWidget *today_, *overdue_, *waits_, *cache_;
};
} // namespace clinic::gui

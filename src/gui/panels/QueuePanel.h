#pragma once
#include "PanelBase.h"
namespace clinic::gui {
class QueuePanel final : public PanelBase {
  public:
    QueuePanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void refresh() override;
    void resetMessage();

  private:
    QComboBox *patient_;
    QTableWidget *table_;
    QLabel *lastCalled_;
};
} // namespace clinic::gui

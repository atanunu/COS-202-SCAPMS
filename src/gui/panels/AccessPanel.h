#pragma once
#include "PanelBase.h"
namespace clinic::gui {
class AccessPanel final : public PanelBase {
  public:
    AccessPanel(controller::ClinicManager &, MutationHandler, std::function<void()> secureWorkspace,
                QWidget *parent = nullptr);
    void refresh() override;

  private:
    QLabel *state_;
    QTableWidget *users_, *audit_;
    void addAccount();
    void changePassword();
};
} // namespace clinic::gui

#pragma once
#include "PanelBase.h"
#include "gui/PatientTableModel.h"
namespace clinic::gui {
class PatientsPanel final : public PanelBase {
  public:
    PatientsPanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void refresh() override;
    void beforeChange() override;
    void editPatient(bool edit);

  private:
    PatientTableModel *model_;
    QTableView *table_;
    QCheckBox *archived_;
    QLabel *details_;
    QString selected_;
    int scroll_ = 0;
    QString selection() const;
};
} // namespace clinic::gui

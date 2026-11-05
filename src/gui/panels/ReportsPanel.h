#pragma once
#include "PanelBase.h"
#include "controller/ReportService.h"
namespace clinic::gui {
class ReportsPanel final : public PanelBase {
  public:
    ReportsPanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void refresh() override;

  private:
    QComboBox *type_;
    QDateEdit *from_, *to_;
    QCheckBox *allDates_;
    QLabel *title_, *summary_;
    QTableWidget *table_;
    controller::Report current_;
    void exportPdf();
    void print();
};
} // namespace clinic::gui

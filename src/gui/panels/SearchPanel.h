#pragma once
#include "PanelBase.h"
#include "gui/PatientTableModel.h"
namespace clinic::gui {
class SearchPanel final : public PanelBase {
  public:
    SearchPanel(controller::ClinicManager &, MutationHandler, QWidget *parent = nullptr);
    void beforeChange() override;
    void refresh() override;

  private:
    PatientTableModel *model_;
    QTableView *table_;
    QComboBox *field_, *algorithm_, *sortField_, *sortAlgorithm_;
    QLineEdit *text_;
    QLabel *info_;
    QCheckBox *archived_;
    enum class ViewMode { All, Search, Sort };
    ViewMode mode_ = ViewMode::All;
    QString committedQuery_, selected_;
    int committedField_ = 0, committedAlgorithm_ = 0, committedSortField_ = 0,
        committedSortAlgorithm_ = 0, scroll_ = 0;
    void rebuild();
    void benchmark();
};
} // namespace clinic::gui

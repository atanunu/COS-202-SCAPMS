#pragma once
#include "controller/ClinicManager.h"
#include <QtWidgets>
#include <functional>
namespace clinic::gui {
using MutationHandler = std::function<void(const std::function<void()> &, const QString &)>;
class PanelBase : public QWidget {
  public:
    PanelBase(controller::ClinicManager &manager, MutationHandler mutate, QWidget *parent = nullptr)
        : QWidget(parent), manager_(manager), mutate_(std::move(mutate)) {}
    virtual void refresh() = 0;
    virtual void beforeChange() {}

  protected:
    controller::ClinicManager &manager_;
    MutationHandler mutate_;
    bool updating_ = false;
};
QVBoxLayout *panelLayout(QWidget *, const QString &, const QString &);
QPushButton *actionButton(const QString &, const QString &, bool primary = false);
QTableWidget *makeTable(const QStringList &, const QString &);
void configureTable(QTableView *);
void fillTable(QTableWidget *, const QList<QStringList> &);
QString selectedId(QTableWidget *);
void selectId(QTableWidget *, const QString &);
void fillPatientChoices(QComboBox *, const model::ClinicDatabase &);
void showPatientHistory(QWidget *, controller::ClinicManager &, const QString &);
} // namespace clinic::gui

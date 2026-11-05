#pragma once
#include "algorithms/Algorithms.h"
#include <QAbstractTableModel>
namespace clinic::gui {
class PatientTableModel final : public QAbstractTableModel {
  public:
    explicit PatientTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &, int role = Qt::DisplayRole) const override;
    QVariant headerData(int, Qt::Orientation, int role = Qt::DisplayRole) const override;
    void setPatients(algorithms::PatientView patients);
    QString idAt(int row) const;
    void setClinicalAccess(bool allowed) {
        clinicalAccess_ = allowed;
    }

  private:
    algorithms::PatientView patients_;
    bool clinicalAccess_ = false;
};
} // namespace clinic::gui

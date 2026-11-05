#include "PatientTableModel.h"
#include <QColor>
namespace clinic::gui {
int PatientTableModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(patients_.size());
}
int PatientTableModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : 5;
}
QVariant PatientTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    const auto *p = patients_[static_cast<size_t>(index.row())];
    if (role == Qt::ToolTipRole)
        return clinicalAccess_ ? model::processPatient(*p) : QString("Clinical details restricted");
    if (role == Qt::ForegroundRole && index.column() == 3) {
        if (p->category() == model::PatientCategory::Emergency)
            return QColor("#b43d34");
        if (p->category() == model::PatientCategory::FollowUp)
            return QColor("#6652a3");
        return QColor("#147368");
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
    case 0:
        return p->id();
    case 1:
        return p->details().name;
    case 2:
        return p->details().age;
    case 3:
        return model::categoryName(p->category());
    case 4:
        return p->details().contact;
    }
    return {};
}
QVariant PatientTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Vertical)
        return section + 1;
    const QStringList headers{"Patient ID", "Full name", "Age", "Category", "Contact"};
    if (section >= 0 && section < headers.size())
        return headers[section];
    return {};
}
void PatientTableModel::setPatients(algorithms::PatientView patients) {
    beginResetModel();
    patients_ = std::move(patients);
    endResetModel();
}
QString PatientTableModel::idAt(int row) const {
    return row >= 0 && row < rowCount() ? patients_[static_cast<size_t>(row)]->id() : QString();
}
} // namespace clinic::gui

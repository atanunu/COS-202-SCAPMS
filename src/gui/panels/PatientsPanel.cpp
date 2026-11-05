#include "PatientsPanel.h"
#include "gui/PatientDialog.h"
namespace clinic::gui {
PatientsPanel::PatientsPanel(controller::ClinicManager &manager, MutationHandler mutation,
                             QWidget *parent)
    : PanelBase(manager, std::move(mutation), parent) {
    auto *layout = panelLayout(
        this, "Patient records",
        "Review history, update details, and archive records without losing previous visits.");
    auto *row = new QHBoxLayout;
    auto *add = actionButton("+ Register patient", "addPatient", true);
    auto *edit = actionButton("Edit selected", "editPatient");
    auto *history = actionButton("View history", "patientHistory");
    auto *archive = actionButton("Archive selected", "deletePatient");
    auto *restore = actionButton("Restore selected", "restorePatient");
    archived_ = new QCheckBox("Include archived");
    archived_->setObjectName("includeArchived");
    for (auto *b : {add, edit, history, archive, restore})
        row->addWidget(b);
    row->addWidget(archived_);
    layout->addLayout(row);
    model_ = new PatientTableModel(this);
    table_ = new QTableView;
    table_->setObjectName("patientsTable");
    table_->setModel(model_);
    configureTable(table_);
    layout->addWidget(table_, 1);
    details_ = new QLabel("Select a record to see details.");
    details_->setWordWrap(true);
    details_->setMinimumHeight(50);
    layout->addWidget(details_);
    connect(add, &QPushButton::clicked, this, [this] { editPatient(false); });
    connect(edit, &QPushButton::clicked, this, [this] { editPatient(true); });
    connect(history, &QPushButton::clicked, this,
            [this] { showPatientHistory(this, manager_, selection()); });
    connect(table_, &QTableView::doubleClicked, this,
            [this] { showPatientHistory(this, manager_, selection()); });
    connect(archived_, &QCheckBox::toggled, this, [this] {
        beforeChange();
        refresh();
    });
    connect(archive, &QPushButton::clicked, this, [this] {
        const auto id = selection();
        if (id.isEmpty())
            return;
        const auto *p = manager_.database().patient(id);
        if (QMessageBox::question(this, "Archive patient",
                                  "Archive " + p->details().name +
                                      "? Active bookings will be cancelled; completed history is "
                                      "retained. Undo is available.") == QMessageBox::Yes)
            mutate_([&] { manager_.deletePatient(id); }, "Patient archived.");
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        const auto id = selection();
        mutate_([&] { manager_.restorePatient(id); },
                "Patient restored. Cancelled appointments stay cancelled.");
    });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (updating_)
            return;
        selected_ = selection();
        const auto *p = manager_.database().patient(selected_);
        if (p) {
            details_->setText(
                p->details().name + " / " +
                (manager_.database().archivedPatients.contains(p->id()) ? "Archived / " : "") +
                (manager_.canViewClinicalNotes() ? model::processPatient(*p)
                                                 : "Clinical details restricted."));
            manager_.access(p->id());
        }
    });
}
QString PatientsPanel::selection() const {
    return model_->idAt(table_->currentIndex().row());
}
void PatientsPanel::beforeChange() {
    selected_ = selection();
    scroll_ = table_->verticalScrollBar()->value();
    updating_ = true;
    model_->setPatients({});
}
void PatientsPanel::refresh() {
    model_->setClinicalAccess(manager_.canViewClinicalNotes());
    updating_ = true;
    algorithms::PatientView view;
    for (const auto &p : manager_.database().patients)
        if (archived_->isChecked() || !manager_.database().archivedPatients.contains(p->id()))
            view.push_back(p.get());
    model_->setPatients(std::move(view));
    for (int i = 0; i < model_->rowCount(); ++i)
        if (model_->idAt(i) == selected_)
            table_->selectRow(i);
    table_->verticalScrollBar()->setValue(scroll_);
    const auto *p = manager_.database().patient(selected_);
    details_->setText(p ? p->details().name + " / " +
                              (manager_.canViewClinicalNotes() ? model::processPatient(*p)
                                                               : "Clinical details restricted.")
                        : "Select a record to see details.");
    for (const auto &name : {"addPatient", "editPatient"})
        findChild<QPushButton *>(name)->setEnabled(manager_.canEdit());
    for (const auto &name : {"deletePatient", "restorePatient"})
        findChild<QPushButton *>(name)->setEnabled(manager_.isAdmin());
    updating_ = false;
}
void PatientsPanel::editPatient(bool edit) {
    const auto id = selection();
    const auto *p = edit ? manager_.database().patient(id) : nullptr;
    if (edit && !p) {
        QMessageBox::information(this, "Edit patient", "Select a patient first.");
        return;
    }
    PatientDialog dialog(manager_.suggestedPatientId(), p, this);
    dialog.configureAccess(manager_.canViewClinicalNotes(), manager_.database());
    if (dialog.exec() == QDialog::Accepted) {
        const auto d = dialog.details();
        const auto category = dialog.category();
        const auto matches = manager_.possibleDuplicates(d, edit ? id : QString());
        if (!matches.isEmpty() &&
            QMessageBox::question(this, "Possible duplicate patient",
                                  "These records share the name and age, or contact:\n" +
                                      matches.join("\n") + "\nContinue with this record?",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes)
            return;
        mutate_(
            [&] {
                if (edit)
                    manager_.editPatient(id, category, d);
                else
                    manager_.addPatient(category, d);
            },
            "Patient record saved.");
    }
}
} // namespace clinic::gui

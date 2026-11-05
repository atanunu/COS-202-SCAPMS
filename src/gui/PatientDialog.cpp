#include "PatientDialog.h"
#include <QtWidgets>
namespace clinic::gui {
PatientDialog::PatientDialog(const QString &suggestedId, const model::PatientRecord *patient,
                             QWidget *parent)
    : QDialog(parent) {
    setObjectName("patientDialog");
    setWindowTitle(patient ? "Edit patient" : "Register patient");
    setMinimumWidth(500);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(patient ? "Update patient record" : "Register a new patient");
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    auto *subtitle = new QLabel("All visible fields are required.");
    layout->addWidget(subtitle);
    auto *form = new QFormLayout;
    form->setSpacing(14);
    id_ = new QLineEdit(suggestedId);
    id_->setObjectName("patientId");
    name_ = new QLineEdit;
    name_->setObjectName("patientName");
    name_->setMaxLength(120);
    age_ = new QSpinBox;
    age_->setRange(0, 120);
    age_->setObjectName("patientAge");
    contact_ = new QLineEdit;
    contact_->setObjectName("patientContact");
    contact_->setMaxLength(160);
    reason_ = new QLineEdit;
    reason_->setObjectName("patientReason");
    reason_->setMaxLength(1000);
    category_ = new QComboBox;
    category_->setObjectName("patientCategory");
    category_->addItems({"General", "Emergency", "Follow-up"});
    urgency_ = new QSpinBox;
    urgency_->setRange(1, 5);
    urgency_->setObjectName("patientUrgency");
    previous_ = new QLineEdit;
    previous_->setObjectName("previousVisit");
    previous_->setMaxLength(120);
    urgencyLabel_ = new QLabel("Urgency (5 = highest)");
    previousLabel_ = new QLabel("Previous visit reference");
    form->addRow("Patient ID", id_);
    form->addRow("Full name", name_);
    form->addRow("Age", age_);
    form->addRow("Contact", contact_);
    form->addRow("Patient category", category_);
    form->addRow("Reason for visit", reason_);
    form->addRow(urgencyLabel_, urgency_);
    form->addRow(previousLabel_, previous_);
    layout->addLayout(form);
    error_ = new QLabel;
    error_->setObjectName("formError");
    error_->setStyleSheet("color:#b43d34");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->setObjectName("patientButtons");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        try {
            auto valid = model::makePatient(category(), details());
            Q_UNUSED(valid);
            accept();
        } catch (const std::exception &e) {
            error_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(category_, &QComboBox::currentIndexChanged, this, [this] { updateFields(); });
    if (patient) {
        const auto &d = patient->details();
        id_->setText(d.id);
        id_->setReadOnly(true);
        name_->setText(d.name);
        age_->setValue(d.age);
        contact_->setText(d.contact);
        reason_->setText(d.reason);
        urgency_->setValue(d.urgency);
        previous_->setText(d.previousVisit);
        category_->setCurrentIndex(static_cast<int>(patient->category()));
    }
    updateFields();
}
void PatientDialog::updateFields() {
    const bool emergency = clinicalAccess_ && category_->currentIndex() == 1,
               followUp = clinicalAccess_ && category_->currentIndex() == 2;
    if (previousChoices_)
        previousChoices_->setVisible(followUp);
    urgency_->setVisible(emergency);
    urgencyLabel_->setVisible(emergency);
    previous_->setVisible(followUp);
    previousLabel_->setVisible(followUp);
}
model::PatientDetails PatientDialog::details() const {
    return {id_->text(),     name_->text(),     age_->value(),    contact_->text(),
            reason_->text(), urgency_->value(), previous_->text()};
}
model::PatientCategory PatientDialog::category() const {
    return static_cast<model::PatientCategory>(category_->currentIndex());
}
void PatientDialog::configureAccess(bool allowed, const model::ClinicDatabase &db) {
    clinicalAccess_ = allowed;
    auto *form = findChild<QFormLayout *>();
    for (auto *field : {static_cast<QWidget *>(reason_), static_cast<QWidget *>(category_)}) {
        field->setVisible(allowed);
        if (auto *label = form->labelForField(field))
            label->setVisible(allowed);
    }
    if (!allowed) {
        if (reason_->text().isEmpty())
            reason_->setText("Awaiting clinician assessment");
        setWindowTitle("Patient demographics");
    } else {
        previous_->setReadOnly(true);
        previous_->setPlaceholderText("Choose a completed visit below");
        previousChoices_ = new QComboBox;
        previousChoices_->setObjectName("previousVisitChoices");
        previousChoices_->addItem("Choose a completed visit", {});
        if (!previous_->text().isEmpty())
            previousChoices_->addItem("Existing reference: " + previous_->text(),
                                      previous_->text());
        for (const auto &visit : db.appointments)
            if (visit.patientId == id_->text() &&
                visit.status == model::AppointmentStatus::Completed)
                previousChoices_->addItem(visit.id + " / " + visit.start.toString("dd MMM yyyy") +
                                              " / " + visit.service,
                                          visit.id);
        auto selected = previousChoices_->findData(previous_->text());
        if (selected >= 0)
            previousChoices_->setCurrentIndex(selected);
        form->addRow(previousChoices_);
        connect(previousChoices_, &QComboBox::currentIndexChanged, this,
                [this] { previous_->setText(previousChoices_->currentData().toString()); });
        if (previousChoices_->count() == 1) {
            auto *items = qobject_cast<QStandardItemModel *>(category_->model());
            if (items)
                items->item(2)->setEnabled(false);
            previous_->setToolTip("Register first, complete a visit, then select Follow-up when "
                                  "editing this patient.");
        }
    }
    updateFields();
}
} // namespace clinic::gui

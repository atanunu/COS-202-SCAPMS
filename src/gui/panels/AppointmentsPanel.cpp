#include "AppointmentsPanel.h"
namespace clinic::gui {
AppointmentsPanel::AppointmentsPanel(controller::ClinicManager &m, MutationHandler f,
                                     QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(
        this, "Appointments",
        "Schedule > Check in > Call next > Complete consultation. All changes support undo.");
    auto *form = new QGridLayout;
    patient_ = new QComboBox;
    patient_->setObjectName("appointmentPatient");
    service_ = new QComboBox;
    service_->setObjectName("appointmentService");
    service_->setEditable(true);
    service_->addItems(
        {"General consultation", "Follow-up review", "Emergency review", "Health screening"});
    time_ = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600));
    time_->setObjectName("appointmentTime");
    time_->setCalendarPopup(true);
    time_->setDisplayFormat("dd MMM yyyy HH:mm");
    duration_ = new QSpinBox;
    duration_->setObjectName("appointmentDuration");
    duration_->setRange(5, 240);
    duration_->setValue(30);
    duration_->setSuffix(" min");
    const QStringList labels{"Patient", "Service", "Date and time", "Duration"};
    for (int i = 0; i < 4; ++i)
        form->addWidget(new QLabel(labels[i]), 0, i);
    form->addWidget(patient_, 1, 0);
    form->addWidget(service_, 1, 1);
    form->addWidget(time_, 1, 2);
    form->addWidget(duration_, 1, 3);
    clinician_ = new QComboBox;
    room_ = new QComboBox;
    clinician_->setObjectName("appointmentClinician");
    room_->setObjectName("appointmentRoom");
    form->addWidget(new QLabel("Clinician"), 2, 0);
    form->addWidget(new QLabel("Room"), 2, 2);
    form->addWidget(clinician_, 3, 0, 1, 2);
    form->addWidget(room_, 3, 2, 1, 2);
    layout->addLayout(form);
    auto *actions = new QGridLayout;
    auto *book = actionButton("+ Schedule visit", "scheduleAppointment", true);
    auto *move = actionButton("Reschedule", "rescheduleAppointment");
    auto *check = actionButton("Check in", "checkInAppointment", true);
    auto *complete = actionButton("Complete consultation", "completeAppointment");
    auto *cancel = actionButton("Cancel selected", "cancelAppointment");
    int actionIndex = 0;
    for (auto *b : {book, move, check, complete, cancel}) {
        actions->addWidget(b, actionIndex / 4, actionIndex % 4);
        ++actionIndex;
    }
    auto *noShow = actionButton("No-show", "noShowAppointment");
    auto *correct = actionButton("Correct notes", "correctAppointment");
    actions->addWidget(noShow, 1, 1);
    actions->addWidget(correct, 1, 2);
    layout->addLayout(actions);
    auto *filters = new QHBoxLayout;
    status_ = new QComboBox;
    status_->setObjectName("appointmentStatusFilter");
    status_->addItems({"All statuses", "Scheduled", "Checked in", "In consultation", "Completed",
                       "Cancelled", "No-show"});
    today_ = new QCheckBox("Today only");
    today_->setObjectName("appointmentsTodayOnly");
    filters->addWidget(new QLabel("FILTER"));
    filters->addWidget(status_);
    filters->addWidget(today_);
    filters->addStretch();
    layout->addLayout(filters);
    table_ = makeTable({"Appointment", "Patient", "Service", "Date / time", "Duration", "Status"},
                       "appointmentsTable");
    layout->addWidget(table_, 1);
    connect(book, &QPushButton::clicked, this, [this] {
        const auto id = patient_->currentData().toString();
        mutate_(
            [&] {
                manager_.schedule(id, service_->currentText(), time_->dateTime(),
                                  duration_->value(), clinician_->currentData().toString(),
                                  room_->currentData().toString());
            },
            "Appointment scheduled.");
    });
    connect(move, &QPushButton::clicked, this, &AppointmentsPanel::reschedule);
    connect(check, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(table_);
        mutate_([&] { manager_.checkIn(id); }, "Patient checked in and added to waiting queue.");
    });
    connect(cancel, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(table_);
        if (id.isEmpty()) {
            QMessageBox::information(this, "Cancel visit", "Select an appointment first.");
            return;
        }
        bool ok = false;
        const auto reason = QInputDialog::getMultiLineText(this, "Cancel appointment",
                                                           "Cancellation reason", {}, &ok);
        if (ok)
            mutate_([&] { manager_.cancel(id, reason); }, "Appointment cancelled.");
    });
    connect(complete, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(table_);
        if (id.isEmpty())
            return;
        bool accepted = false;
        const auto notes = QInputDialog::getMultiLineText(this, "Complete consultation",
                                                          "Consultation notes", {}, &accepted);
        if (accepted)
            mutate_([&] { manager_.complete(id, notes); }, "Consultation completed.");
    });
    connect(noShow, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(table_);
        bool ok = false;
        if (id.isEmpty()) {
            QMessageBox::information(this, "No-show", "Select an appointment first.");
            return;
        }
        const auto reason = QInputDialog::getMultiLineText(this, "Mark no-show",
                                                           "Reason / contact attempt", {}, &ok);
        if (ok)
            mutate_([&] { manager_.noShow(id, reason); }, "Visit marked no-show.");
    });
    connect(correct, &QPushButton::clicked, this, [this] {
        if (!manager_.canViewClinicalNotes())
            return;
        const auto id = selectedId(table_);
        const auto *a = manager_.database().appointment(id);
        if (!a || a->status != model::AppointmentStatus::Completed) {
            QMessageBox::information(this, "Correct notes", "Select a completed consultation.");
            return;
        }
        bool ok = false;
        auto notes = QInputDialog::getMultiLineText(this, "Correct consultation notes",
                                                    "Replacement notes", a->consultationNotes, &ok);
        if (!ok)
            return;
        auto reason = QInputDialog::getMultiLineText(this, "Correction reason",
                                                     "Why are the notes being corrected?", {}, &ok);
        if (ok)
            mutate_([&] { manager_.correctNotes(id, notes, reason); },
                    "Notes corrected; previous notes retained in history.");
    });
    connect(status_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(today_, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(table_, &QTableWidget::doubleClicked, this, [this] {
        const auto *a = manager_.database().appointment(selectedId(table_));
        if (a)
            showPatientHistory(this, manager_, a->patientId);
    });
}
void AppointmentsPanel::refresh() {
    fillPatientChoices(patient_, manager_.database());
    auto fill = [](QComboBox *combo, const QStringList &values) {
        const auto old = combo->currentData().toString();
        combo->clear();
        combo->addItem("Unassigned", {});
        for (const auto &v : values)
            combo->addItem(v, v);
        combo->setCurrentIndex(qMax(0, combo->findData(old)));
    };
    fill(clinician_, manager_.database().clinicians);
    fill(room_, manager_.database().rooms);
    QList<QStringList> rows;
    for (const auto &a : manager_.database().appointments) {
        if (today_->isChecked() && a.start.toLocalTime().date() != QDate::currentDate())
            continue;
        if (status_->currentIndex() > 0 && model::statusName(a.status) != status_->currentText())
            continue;
        rows.append({a.id, manager_.database().patient(a.patientId)->details().name, a.service,
                     a.start.toLocalTime().toString("dd MMM yyyy HH:mm"),
                     QString::number(a.durationMinutes) + " min", model::statusName(a.status)});
    }
    fillTable(table_, rows);
    for (const auto &name : {"scheduleAppointment", "rescheduleAppointment", "checkInAppointment",
                             "cancelAppointment", "noShowAppointment"})
        findChild<QPushButton *>(name)->setEnabled(manager_.canEdit());
    findChild<QPushButton *>("completeAppointment")->setEnabled(manager_.canViewClinicalNotes());
    findChild<QPushButton *>("correctAppointment")->setEnabled(manager_.canViewClinicalNotes());
}
void AppointmentsPanel::reschedule() {
    const auto id = selectedId(table_);
    const auto *a = manager_.database().appointment(id);
    if (!a) {
        QMessageBox::information(this, "Reschedule", "Select an appointment first.");
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName("rescheduleDialog");
    dialog.setWindowTitle("Reschedule " + id);
    auto *form = new QFormLayout(&dialog);
    auto *time = new QDateTimeEdit(a->start);
    time->setObjectName("rescheduleTime");
    time->setCalendarPopup(true);
    time->setDisplayFormat("dd MMM yyyy HH:mm");
    auto *minutes = new QSpinBox;
    minutes->setObjectName("rescheduleDuration");
    minutes->setRange(5, 240);
    minutes->setValue(a->durationMinutes);
    form->addRow("New date / time", time);
    form->addRow("Duration (minutes)", minutes);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        mutate_([&] { manager_.reschedule(id, time->dateTime(), minutes->value()); },
                "Appointment rescheduled.");
}
} // namespace clinic::gui

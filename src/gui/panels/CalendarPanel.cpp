#include "CalendarPanel.h"
#include "algorithms/Algorithms.h"
namespace clinic::gui {
CalendarPanel::CalendarPanel(controller::ClinicManager &m, MutationHandler f, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(this, "Clinician calendar and rooms",
                               "Choose a day and filter by clinician or room. Active visits cannot "
                               "overlap for the same resource.");
    auto *top = new QHBoxLayout;
    calendar_ = new QCalendarWidget;
    calendar_->setObjectName("resourceCalendar");
    calendar_->setGridVisible(true);
    top->addWidget(calendar_);
    auto *options = new QVBoxLayout;
    clinician_ = new QComboBox;
    room_ = new QComboBox;
    clinician_->setObjectName("calendarClinician");
    room_->setObjectName("calendarRoom");
    options->addWidget(new QLabel("Clinician"));
    options->addWidget(clinician_);
    options->addWidget(new QLabel("Room"));
    options->addWidget(room_);
    auto *addClinician = actionButton("Add clinician", "addClinician");
    auto *addRoom = actionButton("Add room", "addRoom");
    auto *assign = actionButton("Allocate selected visit", "allocateResources", true);
    options->addWidget(addClinician);
    options->addWidget(addRoom);
    options->addWidget(assign);
    options->addStretch();
    top->addLayout(options, 1);
    layout->addLayout(top);
    summary_ = new QLabel;
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    table_ =
        makeTable({"Visit", "Time", "Patient", "Clinician", "Room", "Status"}, "calendarTable");
    layout->addWidget(table_, 1);
    connect(calendar_, &QCalendarWidget::selectionChanged, this, [this] { refresh(); });
    connect(clinician_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(room_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    for (auto pair : {qMakePair(addClinician, false), qMakePair(addRoom, true)}) {
        connect(pair.first, &QPushButton::clicked, this, [this, isRoom = pair.second] {
            bool ok = false;
            auto name = QInputDialog::getText(this, isRoom ? "Add room" : "Add clinician", "Name",
                                              QLineEdit::Normal, {}, &ok);
            if (ok)
                mutate_([&] { manager_.addResource(name, isRoom); }, "Resource added.");
        });
    }
    connect(assign, &QPushButton::clicked, this, &CalendarPanel::allocate);
    connect(table_, &QTableWidget::doubleClicked, this, [this] {
        if (auto *a = manager_.database().appointment(selectedId(table_)))
            showPatientHistory(this, manager_, a->patientId);
    });
}
void CalendarPanel::refresh() {
    const auto &db = manager_.database();
    auto fill = [](QComboBox *combo, const QStringList &names) {
        const QSignalBlocker blocker(combo);
        auto current = combo->currentData().toString();
        combo->clear();
        combo->addItem("All", {});
        for (const auto &name : names)
            combo->addItem(name, name);
        auto index = combo->findData(current);
        combo->setCurrentIndex(index < 0 ? 0 : index);
    };
    fill(clinician_, db.clinicians);
    fill(room_, db.rooms);
    std::vector<const model::Appointment *> visits;
    for (const auto &a : db.appointments) {
        if (a.start.toLocalTime().date() != calendar_->selectedDate())
            continue;
        if (!clinician_->currentData().toString().isEmpty() &&
            a.clinician != clinician_->currentData().toString())
            continue;
        if (!room_->currentData().toString().isEmpty() && a.room != room_->currentData().toString())
            continue;
        visits.push_back(&a);
    }
    algorithms::mergeSort(visits, [](auto a, auto b) { return a->start < b->start; });
    QList<QStringList> rows;
    for (auto *a : visits)
        rows.append({a->id,
                     a->start.toLocalTime().toString("HH:mm") + " - " +
                         a->end().toLocalTime().toString("HH:mm"),
                     db.patient(a->patientId)->details().name,
                     a->clinician.isEmpty() ? "Unassigned" : a->clinician,
                     a->room.isEmpty() ? "Unassigned" : a->room, model::statusName(a->status)});
    fillTable(table_, rows);
    summary_->setText(
        rows.isEmpty()
            ? "No visits match this day and resource filter. Schedule a visit on the Appointments "
              "page."
            : QString("%1 visits on %2. Unassigned visits remain visible when All is selected.")
                  .arg(rows.size())
                  .arg(calendar_->selectedDate().toString("dd MMM yyyy")));
    findChild<QPushButton *>("addClinician")->setEnabled(manager_.isAdmin());
    findChild<QPushButton *>("addRoom")->setEnabled(manager_.isAdmin());
    findChild<QPushButton *>("allocateResources")
        ->setEnabled(manager_.canEdit() && !rows.isEmpty());
}
void CalendarPanel::allocate() {
    const auto id = selectedId(table_);
    const auto *a = manager_.database().appointment(id);
    if (!a) {
        QMessageBox::information(this, "Choose a visit", "Select a visit in the calendar first.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Allocate " + id);
    auto *form = new QFormLayout(&dialog);
    auto *clinician = new QComboBox;
    auto *room = new QComboBox;
    for (auto *c : {clinician, room})
        c->addItem("Unassigned", {});
    for (const auto &x : manager_.database().clinicians)
        clinician->addItem(x, x);
    for (const auto &x : manager_.database().rooms)
        room->addItem(x, x);
    clinician->setCurrentIndex(qMax(0, clinician->findData(a->clinician)));
    room->setCurrentIndex(qMax(0, room->findData(a->room)));
    form->addRow("Clinician", clinician);
    form->addRow("Room", room);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        mutate_(
            [&] {
                manager_.assignResources(id, clinician->currentData().toString(),
                                         room->currentData().toString());
            },
            "Resources allocated.");
}
} // namespace clinic::gui
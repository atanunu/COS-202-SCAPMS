#include "DashboardPanel.h"
#include "algorithms/Algorithms.h"
namespace clinic::gui {
DashboardPanel::DashboardPanel(controller::ClinicManager &m, MutationHandler f, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(this, "Today's care at a glance",
                               "Upcoming visits, overdue appointments and live waiting times.");
    auto *metrics = new QHBoxLayout;
    auto card = [&](const QString &name, QLabel *&count) {
        auto *frame = new QFrame;
        frame->setObjectName("card");
        auto *body = new QVBoxLayout(frame);
        body->addWidget(new QLabel(name));
        count = new QLabel("0");
        count->setObjectName("metric");
        body->addWidget(count);
        metrics->addWidget(frame);
    };
    card("ACTIVE PATIENTS", patients_);
    card("PENDING VISITS", pending_);
    card("WAITING NOW", waiting_);
    card("COMPLETED TODAY", completed_);
    layout->addLayout(metrics);
    reminder_ = new QLabel;
    reminder_->setWordWrap(true);
    reminder_->setObjectName("dashboardReminder");
    layout->addWidget(reminder_);
    auto *tabs = new QTabWidget;
    tabs->setObjectName("dashboardLists");
    today_ = makeTable({"Appointment", "Patient", "Time", "Service", "Status"}, "todayTable");
    overdue_ = makeTable({"Appointment", "Patient", "Scheduled for", "Overdue"}, "overdueTable");
    waits_ = makeTable({"Patient ID", "Patient", "Appointment", "Waiting"}, "dashboardWaiting");
    cache_ = makeTable({"Patient ID", "Patient", "Category", "Record views"}, "cacheTable");
    tabs->addTab(today_, "Today's appointments");
    tabs->addTab(overdue_, "Overdue visits");
    tabs->addTab(waits_, "Waiting times");
    tabs->addTab(cache_, "Frequently accessed");
    layout->addWidget(tabs, 1);
    for (auto *table : {today_, overdue_})
        connect(table, &QTableWidget::doubleClicked, this, [this, table] {
            const auto *a = manager_.database().appointment(selectedId(table));
            if (a)
                showPatientHistory(this, manager_, a->patientId);
        });
    connect(cache_, &QTableWidget::doubleClicked, this,
            [this] { showPatientHistory(this, manager_, selectedId(cache_)); });
}
void DashboardPanel::refresh() {
    const auto &db = manager_.database();
    const auto now = QDateTime::currentDateTime();
    patients_->setText(QString::number(db.patients.size() - db.archivedPatients.size()));
    waiting_->setText(QString::number(db.waiting.size()));
    int pending = 0, completed = 0, soon = 0;
    QList<QStringList> today, overdue, waits, frequent;
    std::vector<const model::Appointment *> appointments;
    for (const auto &a : db.appointments)
        appointments.push_back(&a);
    algorithms::mergeSort(appointments,
                          [](const auto *a, const auto *b) { return a->start < b->start; });
    for (const auto *a : appointments) {
        if (model::isPending(a->status))
            ++pending;
        if (a->status == model::AppointmentStatus::Scheduled && a->start >= now &&
            a->start <= now.addSecs(3600))
            ++soon;
        if (a->start.toLocalTime().date() == QDate::currentDate()) {
            today.append({a->id, db.patient(a->patientId)->details().name,
                          a->start.toLocalTime().toString("HH:mm"), a->service,
                          model::statusName(a->status)});
            if (a->status == model::AppointmentStatus::Completed)
                ++completed;
        }
        if (a->status == model::AppointmentStatus::Scheduled && a->start < now)
            overdue.append({a->id, db.patient(a->patientId)->details().name,
                            a->start.toLocalTime().toString("dd MMM HH:mm"),
                            QString::number(a->start.secsTo(now) / 60) + " min"});
    }
    auto queue = db.waiting;
    while (!queue.empty()) {
        const auto &e = queue.front();
        waits.append({e.patientId, db.patient(e.patientId)->details().name,
                      e.appointmentId.isEmpty() ? "Walk-in" : e.appointmentId,
                      QString::number(qMax<qint64>(0, e.arrivedAt.secsTo(now) / 60)) + " min"});
        queue.pop();
    }
    std::vector<model::CacheEntry> entries;
    for (const auto &e : db.frequent)
        if (!e.patientId.isEmpty())
            entries.push_back(e);
    algorithms::insertionSort(entries,
                              [](const auto &a, const auto &b) { return a.accesses > b.accesses; });
    for (const auto &e : entries)
        if (const auto *p = db.patient(e.patientId))
            frequent.append({p->id(), p->details().name, model::categoryName(p->category()),
                             QString::number(e.accesses)});
    pending_->setText(QString::number(pending));
    completed_->setText(QString::number(completed));
    reminder_->setText(QString("%1 | %2 visits in the next hour | %3 overdue | %4 waiting")
                           .arg(now.toString("dddd, dd MMMM yyyy"))
                           .arg(soon)
                           .arg(overdue.size())
                           .arg(waits.size()));
    fillTable(today_, today);
    fillTable(overdue_, overdue);
    fillTable(waits_, waits);
    fillTable(cache_, frequent);
}
} // namespace clinic::gui

#include "QueuePanel.h"
namespace clinic::gui {
QueuePanel::QueuePanel(controller::ClinicManager &m, MutationHandler f, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(
        this, "Waiting queue",
        "FIFO order with appointment links and live waiting times. Walk-ins may also join.");
    auto *row = new QHBoxLayout;
    patient_ = new QComboBox;
    patient_->setObjectName("queuePatient");
    row->addWidget(patient_, 1);
    auto *add = actionButton("Add walk-in", "enqueuePatient", true);
    auto *next = actionButton("Call next patient", "callNext", true);
    auto *remove = actionButton("Remove selected", "removeQueued");
    for (auto *b : {add, next, remove})
        row->addWidget(b);
    layout->addLayout(row);
    table_ = makeTable({"Patient ID", "Patient", "Category", "Appointment", "Waiting", "Position"},
                       "queueTable");
    layout->addWidget(table_, 1);
    lastCalled_ = new QLabel;
    lastCalled_->setObjectName("lastCalled");
    lastCalled_->setWordWrap(true);
    layout->addWidget(lastCalled_);
    resetMessage();
    connect(add, &QPushButton::clicked, this, [this] {
        const auto id = patient_->currentData().toString();
        mutate_([&] { manager_.enqueue(id); }, "Walk-in added.");
    });
    connect(next, &QPushButton::clicked, this, [this] {
        QString summary;
        mutate_([&] { summary = manager_.callNext(); }, "Patient called.");
        if (!summary.isEmpty())
            lastCalled_->setText("LAST CALLED\n" + summary);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(table_);
        mutate_([&] { manager_.removeFromQueue(id); },
                "Removed from queue. Linked appointment returned to Scheduled.");
    });
}
void QueuePanel::refresh() {
    fillPatientChoices(patient_, manager_.database());
    auto queue = manager_.database().waiting;
    QList<QStringList> rows;
    int position = 1;
    while (!queue.empty()) {
        const auto &e = queue.front();
        const auto *p = manager_.database().patient(e.patientId);
        const auto minutes = qMax<qint64>(0, e.arrivedAt.secsTo(QDateTime::currentDateTime()) / 60);
        rows.append({e.patientId, p->details().name, model::categoryName(p->category()),
                     e.appointmentId.isEmpty() ? "Walk-in" : e.appointmentId,
                     QString::number(minutes) + " min", QString::number(position++)});
        queue.pop();
    }
    fillTable(table_, rows);
    findChild<QPushButton *>("callNext")->setEnabled(manager_.canViewClinicalNotes());
    for (const auto &name : {"enqueuePatient", "removeQueued"})
        findChild<QPushButton *>(name)->setEnabled(manager_.canEdit());
}
void QueuePanel::resetMessage() {
    lastCalled_->setText("Select Call next to begin the next linked consultation.");
}
} // namespace clinic::gui

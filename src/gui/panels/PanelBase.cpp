#include "PanelBase.h"
namespace clinic::gui {
namespace {
class EmptyState final : public QObject {
    QPointer<QWidget> viewport_;
    QPointer<QAbstractItemModel> model_;
    QPointer<QLabel> label_;

  public:
    explicit EmptyState(QTableView *table)
        : QObject(table->viewport()), viewport_(table->viewport()), model_(table->model()),
          label_(new QLabel(table->viewport())) {
        const auto name = table->objectName();
        label_->setText(
            name == "patientsTable"
                ? "No patient records in this view. Register a patient or include archived records."
            : name == "searchResults"
                ? "No matching patients. Change your search or choose Show all."
                : "No records in this view. Adjust the date or filters, or add a record.");
        label_->setAlignment(Qt::AlignCenter);
        label_->setWordWrap(true);
        label_->setAttribute(Qt::WA_TransparentForMouseEvents);
        label_->setStyleSheet("color:#536b76; background:transparent; padding:24px;");
        table->viewport()->installEventFilter(this);
        QObject::connect(table->model(), &QAbstractItemModel::modelReset, this,
                         [this] { refresh(); });
        QObject::connect(table->model(), &QAbstractItemModel::rowsInserted, this,
                         [this] { refresh(); });
        QObject::connect(table->model(), &QAbstractItemModel::rowsRemoved, this,
                         [this] { refresh(); });
        refresh();
    }
    void refresh() {
        if (!viewport_ || !model_ || !label_)
            return;
        label_->setGeometry(viewport_->rect());
        label_->setVisible(model_->rowCount() == 0);
    }
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Resize)
            refresh();
        return QObject::eventFilter(object, event);
    }
};
} // namespace

QVBoxLayout *panelLayout(QWidget *widget, const QString &title, const QString &subtitle) {
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);
    auto *heading = new QLabel(title);
    heading->setObjectName("pageTitle");
    layout->addWidget(heading);
    auto *caption = new QLabel(subtitle);
    caption->setObjectName("caption");
    caption->setWordWrap(true);
    layout->addWidget(caption);
    return layout;
}
QPushButton *actionButton(const QString &title, const QString &name, bool primary) {
    auto *b = new QPushButton(title);
    b->setObjectName(name);
    b->setAccessibleName(title);
    b->setToolTip(title);
    b->setProperty("primary", primary);
    b->setMinimumHeight(36);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
void configureTable(QTableView *table) {
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(38);
    table->setShowGrid(false);
    table->setSortingEnabled(false);
    new EmptyState(table);
    auto *open = new QShortcut(QKeySequence(Qt::Key_Return), table);
    open->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(open, &QShortcut::activated, table, [table] {
        if (table->currentIndex().isValid())
            emit table->doubleClicked(table->currentIndex());
    });
    table->setAccessibleName(
        "Records table. Use arrow keys to select a row and Enter to open details.");
    table->setWordWrap(true);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}
QTableWidget *makeTable(const QStringList &headers, const QString &name) {
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setObjectName(name);
    configureTable(table);
    return table;
}
QString selectedId(QTableWidget *table) {
    const int row = table->currentRow();
    return row >= 0 && table->item(row, 0) ? table->item(row, 0)->text() : QString();
}
void selectId(QTableWidget *table, const QString &id) {
    for (int i = 0; i < table->rowCount(); ++i)
        if (table->item(i, 0)->text() == id) {
            table->selectRow(i);
            return;
        }
}
void fillTable(QTableWidget *table, const QList<QStringList> &rows) {
    const auto selected = selectedId(table);
    const auto scroll = table->verticalScrollBar()->value();
    table->setAccessibleDescription(rows.isEmpty() ? "No records match this view."
                                                   : QString("%1 records").arg(rows.size()));
    table->setToolTip(rows.isEmpty() ? "No records match this view. Adjust filters or add a record."
                                     : "");
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r)
        for (int c = 0; c < rows[r].size(); ++c) {
            auto *item = new QTableWidgetItem(rows[r][c]);
            table->setItem(r, c, item);
            if (rows[r][c] == "Cancelled" || rows[r][c] == "Emergency" || rows[r][c] == "Archived")
                item->setForeground(QColor("#b43d34"));
            if (rows[r][c] == "Completed")
                item->setForeground(QColor("#147368"));
        }
    selectId(table, selected);
    table->verticalScrollBar()->setValue(scroll);
}
void fillPatientChoices(QComboBox *combo, const model::ClinicDatabase &db) {
    const auto selected = combo->currentData().toString();
    combo->clear();
    for (const auto &p : db.patients)
        if (!db.archivedPatients.contains(p->id()))
            combo->addItem(p->details().name + " / " + p->id(), p->id());
    const int index = combo->findData(selected);
    if (index >= 0)
        combo->setCurrentIndex(index);
}
void showPatientHistory(QWidget *parent, controller::ClinicManager &manager, const QString &id) {
    const auto *patient = manager.database().patient(id);
    if (!patient) {
        QMessageBox::information(parent, "Patient history", "Select a patient first.");
        return;
    }
    manager.access(id);
    QDialog dialog(parent);
    dialog.setObjectName("historyDialog");
    dialog.setWindowTitle("Patient history - " + patient->details().name);
    dialog.resize(950, 570);
    auto *layout = new QVBoxLayout(&dialog);
    auto *heading =
        new QLabel(patient->details().name + " / " + id +
                   (manager.database().archivedPatients.contains(id) ? " / Archived" : ""));
    heading->setObjectName("sectionTitle");
    layout->addWidget(heading);
    auto *summary =
        new QLabel(manager.canViewClinicalNotes()
                       ? model::processPatient(*patient)
                       : "Clinical details are available to clinicians and administrators.");
    summary->setWordWrap(true);
    layout->addWidget(summary);
    auto *table = makeTable(
        {"Appointment", "Date / time", "Service", "Status", "Consultation notes"}, "historyTable");
    QList<QStringList> rows;
    for (const auto &a : manager.database().appointments)
        if (a.patientId == id)
            rows.append({a.id, a.start.toLocalTime().toString("dd MMM yyyy HH:mm"), a.service,
                         model::statusName(a.status),
                         manager.canViewClinicalNotes() ? a.consultationNotes : "Restricted"});
    fillTable(table, rows);
    layout->addWidget(table);
    auto *notes = new QTextEdit;
    notes->setReadOnly(true);
    notes->setMaximumHeight(140);
    notes->setPlaceholderText("Select a visit to read its complete notes.");
    layout->addWidget(notes);
    QObject::connect(table, &QTableWidget::itemSelectionChanged, &dialog, [&] {
        const auto *a = manager.database().appointment(selectedId(table));
        notes->setPlainText(a && manager.canViewClinicalNotes()
                                ? a->consultationNotes + "\n\nChange reason: " + a->changeReason +
                                      "\n\nCorrection history:\n" + a->corrections.join("\n\n")
                                : "Clinical notes restricted.");
    });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(close);
    QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
} // namespace clinic::gui

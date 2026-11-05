#include "SearchPanel.h"
#include <QElapsedTimer>
#include "algorithms/Benchmark.h"
#include <future>
#include <chrono>
namespace clinic::gui {
using namespace algorithms;
SearchPanel::SearchPanel(controller::ClinicManager &m, MutationHandler f, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(this, "Search & sort",
                               "Choose a manual algorithm. Timings include the work described; "
                               "results survive record updates.");
    auto *row = new QHBoxLayout;
    field_ = new QComboBox;
    field_->setObjectName("searchField");
    field_->addItems({"Patient ID", "Patient name", "Category"});
    algorithm_ = new QComboBox;
    algorithm_->setObjectName("searchAlgorithm");
    algorithm_->addItems({"Linear search", "Binary search (ID)"});
    text_ = new QLineEdit;
    text_->setObjectName("searchText");
    text_->setPlaceholderText("ID, name or category");
    auto *search = actionButton("Search", "searchButton", true);
    auto *reset = actionButton("Show all", "resetSearch");
    row->addWidget(field_);
    row->addWidget(algorithm_);
    row->addWidget(text_, 1);
    row->addWidget(search);
    row->addWidget(reset);
    layout->addLayout(row);
    auto *sortRow = new QHBoxLayout;
    sortField_ = new QComboBox;
    sortField_->setObjectName("sortField");
    sortField_->addItems({"Patient name", "Patient ID", "Age"});
    sortAlgorithm_ = new QComboBox;
    sortAlgorithm_->setObjectName("sortAlgorithm");
    sortAlgorithm_->addItems({"Insertion sort", "Merge sort (recursive)"});
    auto *sort = actionButton("Sort all records", "sortButton");
    auto *compare = actionButton("Compare algorithms", "benchmarkButton");
    archived_ = new QCheckBox("Include archived");
    archived_->setObjectName("searchArchived");
    sortRow->addWidget(sortField_);
    sortRow->addWidget(sortAlgorithm_);
    sortRow->addWidget(sort);
    sortRow->addWidget(compare);
    sortRow->addWidget(archived_);
    layout->addLayout(sortRow);
    info_ = new QLabel;
    info_->setObjectName("searchInfo");
    info_->setWordWrap(true);
    info_->setMinimumHeight(45);
    layout->addWidget(info_);
    model_ = new PatientTableModel(this);
    table_ = new QTableView;
    table_->setObjectName("searchResults");
    table_->setModel(model_);
    configureTable(table_);
    layout->addWidget(table_, 1);
    auto execute = [this] {
        if (text_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Search", "Enter an ID, name or category.");
            return;
        }
        committedQuery_ = text_->text().trimmed();
        committedField_ = field_->currentIndex();
        committedAlgorithm_ = algorithm_->currentIndex();
        mode_ = ViewMode::Search;
        rebuild();
    };
    connect(search, &QPushButton::clicked, this, execute);
    connect(text_, &QLineEdit::returnPressed, this, execute);
    connect(sort, &QPushButton::clicked, this, [this] {
        committedSortField_ = sortField_->currentIndex();
        committedSortAlgorithm_ = sortAlgorithm_->currentIndex();
        mode_ = ViewMode::Sort;
        rebuild();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        mode_ = ViewMode::All;
        text_->clear();
        rebuild();
    });
    connect(archived_, &QCheckBox::toggled, this, [this] { rebuild(); });
    connect(field_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index != 0)
            algorithm_->setCurrentIndex(0);
        algorithm_->setEnabled(index == 0);
    });
    connect(compare, &QPushButton::clicked, this, &SearchPanel::benchmark);
    connect(table_, &QTableView::clicked, this, [this](const QModelIndex &index) {
        selected_ = model_->idAt(index.row());
        manager_.access(selected_);
    });
    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        showPatientHistory(this, manager_, model_->idAt(index.row()));
    });
}
void SearchPanel::beforeChange() {
    selected_ = model_->idAt(table_->currentIndex().row());
    scroll_ = table_->verticalScrollBar()->value();
    model_->setPatients({});
}
void SearchPanel::refresh() {
    rebuild();
}
void SearchPanel::rebuild() {
    model_->setClinicalAccess(manager_.canViewClinicalNotes());
    PatientView view;
    for (const auto &p : manager_.database().patients)
        if (archived_->isChecked() || !manager_.database().archivedPatients.contains(p->id()))
            view.push_back(p.get());
    Metrics metrics, preparation;
    QElapsedTimer timer;
    timer.start();
    QString explanation = "All active patient records.";
    if (mode_ == ViewMode::Search) {
        if (committedField_ == 0) {
            const model::PatientRecord *found = nullptr;
            if (committedAlgorithm_ == 1) {
                sortPatients(view, SortField::Id, SortAlgorithm::Merge, &preparation);
                found = binarySearchById(view, committedQuery_, &metrics);
                explanation = QString("Binary search: %1 comparisons; manual ID sort: %2 "
                                      "comparisons. Preparation O(n log n), lookup O(log n).")
                                  .arg(metrics.comparisons)
                                  .arg(preparation.comparisons);
            } else {
                for (const auto *p : view) {
                    ++metrics.comparisons;
                    if (p->id() == committedQuery_) {
                        found = p;
                        break;
                    }
                }
                explanation =
                    QString("Linear ID search: %1 comparisons; O(n).").arg(metrics.comparisons);
            }
            view.clear();
            if (found)
                view.push_back(found);
        } else {
            view = filterPatients(view, committedQuery_, committedField_ == 2);
            explanation =
                QString("Case-insensitive linear substring scan: %1 matches.").arg(view.size());
        }
    } else if (mode_ == ViewMode::Sort) {
        sortPatients(view, static_cast<SortField>(committedSortField_),
                     static_cast<SortAlgorithm>(committedSortAlgorithm_), &metrics);
        explanation =
            QString("%1 | %2 comparisons / %3 moves / recursion depth %4.")
                .arg(committedSortAlgorithm_ == 0
                         ? "Insertion sort: small or nearly sorted lists; worst case O(n^2)."
                         : "Recursive merge sort: stable O(n log n); O(n) temporary space.")
                .arg(metrics.comparisons)
                .arg(metrics.moves)
                .arg(metrics.maxRecursionDepth);
    }
    const double elapsed = timer.nsecsElapsed() / 1000000.0;
    info_->setText(explanation +
                   QString("\n%1 ms including any sort preparation; %2 displayed records.")
                       .arg(elapsed, 0, 'f', 3)
                       .arg(view.size()));
    model_->setPatients(std::move(view));
    for (int i = 0; i < model_->rowCount(); ++i)
        if (model_->idAt(i) == selected_)
            table_->selectRow(i);
    table_->verticalScrollBar()->setValue(scroll_);
}
void SearchPanel::benchmark() {
    auto database = manager_.database();
    if (database.patients.empty() || database.patients.size() > 10000) {
        QMessageBox::information(
            this, "Benchmark",
            "Use 1-10,000 records. Load benchmark-2000.json to compare a larger dataset.");
        return;
    }
    const auto field = static_cast<SortField>(sortField_->currentIndex());
    QProgressDialog progress(
        "Comparing three input orders with five measured runs per algorithm...", QString(), 0, 0,
        this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.show();
    auto result = std::async(std::launch::async, [db = std::move(database), field] {
        return benchmarkSorts(viewOf(db.patients), field, 5);
    });
    while (result.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready)
        QApplication::processEvents();
    progress.close();
    std::vector<BenchmarkResult> results;
    try {
        results = result.get();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Benchmark failed", e.what());
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName("benchmarkDialog");
    dialog.setWindowTitle("Repeated sorting comparison");
    dialog.resize(1000, 450);
    auto *layout = new QVBoxLayout(&dialog);
    auto *table = makeTable({"Input order", "Algorithm", "Records", "Median ms (5 runs)",
                             "Comparisons", "Moves", "Depth"},
                            "benchmarkTable");
    QList<QStringList> rows;
    for (const auto &r : results)
        rows.append(
            {r.order,
             r.algorithm == SortAlgorithm::Insertion ? "Insertion sort" : "Recursive merge sort",
             QString::number(r.records), QString::number(r.medianMs, 'f', 3),
             QString::number(r.metrics.comparisons), QString::number(r.metrics.moves),
             QString::number(r.metrics.maxRecursionDepth)});
    fillTable(table, rows);
    layout->addWidget(table);
    auto *note =
        new QLabel("Identical input per algorithm: sorted, reversed and seeded shuffled. One "
                   "warm-up is excluded; five timed runs determine each median. Input copying is "
                   "excluded. Counts are deterministic; timings depend on this computer.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
} // namespace clinic::gui

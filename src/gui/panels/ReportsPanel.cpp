#include "ReportsPanel.h"
#include "persistence/FileHandler.h"
#include <QPrintDialog>
#include <QPrinter>
namespace clinic::gui {
ReportsPanel::ReportsPanel(controller::ClinicManager &m, MutationHandler f, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(
        this, "Clinic reports",
        "Filter appointment reports by date range, review totals, and export or print.");
    auto *row = new QHBoxLayout;
    type_ = new QComboBox;
    type_->setObjectName("reportType");
    type_->addItems({"Most frequently attended patients", "Patients with pending appointments",
                     "Patient category distribution", "Appointment status summary"});
    from_ = new QDateEdit(QDate::currentDate().addDays(-30));
    from_->setObjectName("reportDate");
    from_->setCalendarPopup(true);
    to_ = new QDateEdit(QDate::currentDate());
    to_->setObjectName("reportEndDate");
    to_->setCalendarPopup(true);
    for (auto *field : {from_, to_})
        field->setDisplayFormat("dd MMM yyyy");
    allDates_ = new QCheckBox("All dates");
    allDates_->setObjectName("reportAllDates");
    allDates_->setChecked(true);
    row->addWidget(type_, 1);
    row->addWidget(from_);
    row->addWidget(new QLabel("to"));
    row->addWidget(to_);
    row->addWidget(allDates_);
    layout->addLayout(row);
    auto *actions = new QHBoxLayout;
    auto *csv = actionButton("Export CSV", "exportReport");
    auto *pdf = actionButton("Export PDF", "exportReportPdf");
    auto *printer = actionButton("Print", "printReport");
    actions->addWidget(csv);
    actions->addWidget(pdf);
    actions->addWidget(printer);
    actions->addStretch();
    layout->addLayout(actions);
    title_ = new QLabel;
    title_->setObjectName("sectionTitle");
    summary_ = new QLabel;
    summary_->setObjectName("reportSummary");
    summary_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(summary_);
    table_ = makeTable({}, "reportsTable");
    layout->addWidget(table_, 1);
    connect(type_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(from_, &QDateEdit::dateChanged, this, [this] { refresh(); });
    connect(to_, &QDateEdit::dateChanged, this, [this] { refresh(); });
    connect(allDates_, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(csv, &QPushButton::clicked, this, [this] {
        if (!manager_.canExportReports())
            return;
        const auto path = QFileDialog::getSaveFileName(this, "Export clinic report",
                                                       "clinic-report.csv", "CSV (*.csv)");
        if (path.isEmpty())
            return;
        try {
            manager_.requireReportExport();
            persistence::FileHandler::writeText(path, controller::reportCsv(current_));
            manager_.recordExport("Export CSV report");
            QMessageBox::information(this, "Report exported", "Saved CSV report to " + path);
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Export failed", e.what());
        }
    });
    connect(pdf, &QPushButton::clicked, this, &ReportsPanel::exportPdf);
    connect(printer, &QPushButton::clicked, this, &ReportsPanel::print);
}
void ReportsPanel::refresh() {
    const bool dated = !allDates_->isChecked() && type_->currentIndex() != 2;
    from_->setEnabled(dated);
    to_->setEnabled(dated);
    try {
        current_ = controller::generateReport(
            manager_.database(), static_cast<controller::ReportType>(type_->currentIndex()),
            dated ? from_->date() : QDate{}, dated ? to_->date() : QDate{});
        title_->setText(current_.title);
        summary_->setText(current_.summary);
        table_->setColumnCount(current_.headers.size());
        table_->setHorizontalHeaderLabels(current_.headers);
        fillTable(table_, current_.rows);
        for (const auto &n : {"exportReport", "exportReportPdf", "printReport"})
            findChild<QPushButton *>(n)->setEnabled(manager_.canExportReports());
    } catch (const std::exception &e) {
        summary_->setText(e.what());
        table_->setRowCount(0);
        for (const auto &n : {"exportReport", "exportReportPdf", "printReport"})
            findChild<QPushButton *>(n)->setEnabled(false);
    }
}
void ReportsPanel::exportPdf() {
    if (!manager_.canExportReports())
        return;
    auto file = QFileDialog::getSaveFileName(this, "Export printable report", "clinic-report.pdf",
                                             "PDF (*.pdf)");
    if (file.isEmpty())
        return;
    if (!file.endsWith(".pdf", Qt::CaseInsensitive))
        file += ".pdf";
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        QMessageBox::warning(this, "Export failed", "Cannot create temporary report.");
        return;
    }
    const auto rendered = temporary.filePath("report.pdf");
    printer.setOutputFileName(rendered);
    printer.setPageOrientation(QPageLayout::Landscape);
    QTextDocument document;
    document.setHtml(controller::reportHtml(current_));
    document.print(&printer);
    QFile input(rendered);
    QSaveFile output(file);
    if (!input.open(QIODevice::ReadOnly) || input.size() == 0 ||
        !output.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export failed", "The PDF could not be written.");
        return;
    }
    const auto bytes = input.readAll();
    if (output.write(bytes) != bytes.size() || !output.commit()) {
        QMessageBox::warning(this, "Export failed", "The PDF could not be committed.");
        return;
    }
    manager_.recordExport("Export PDF report");
    QMessageBox::information(this, "Report exported", "Saved PDF report to " + file);
}
void ReportsPanel::print() {
    if (!manager_.canExportReports())
        return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == QDialog::Accepted) {
        QTextDocument document;
        document.setHtml(controller::reportHtml(current_));
        document.print(&printer);
        manager_.recordExport("Print report");
    }
}
} // namespace clinic::gui

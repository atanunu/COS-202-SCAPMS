#pragma once
#include "model/ClinicDatabase.h"
#include <QStringList>
namespace clinic::controller {
enum class ReportType { Attendance, Pending, Categories, Daily };
struct Report {
    QString title;
    QStringList headers;
    QList<QStringList> rows;
    QString summary;
};
Report generateReport(const model::ClinicDatabase &, ReportType, QDate from = {}, QDate to = {});
QString reportCsv(const Report &);
QString reportHtml(const Report &);
} // namespace clinic::controller

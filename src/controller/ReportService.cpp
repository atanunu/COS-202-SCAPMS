#include "ReportService.h"
#include "algorithms/Algorithms.h"
#include "model/Errors.h"
namespace clinic::controller {
using namespace model;
Report generateReport(const ClinicDatabase &db, ReportType type, QDate from, QDate to) {
    if (from.isValid() && !to.isValid())
        to = from;
    if (from.isValid() && to < from)
        throw ValidationError("The end date cannot be earlier than the start date.");
    auto included = [&](const Appointment &a) {
        const auto d = a.start.toLocalTime().date();
        return !from.isValid() || (d >= from && d <= to);
    };
    Report report;
    const auto range = from.isValid()
                           ? from.toString("dd MMM yyyy") + " - " + to.toString("dd MMM yyyy")
                           : "All dates";
    if (type == ReportType::Attendance) {
        report.title = "Most frequently attended patients";
        report.headers = {"Patient ID", "Patient", "Completed visits", "Record"};
        struct Count {
            QString id, name, state;
            int count;
        };
        std::vector<Count> rows;
        int visits = 0;
        for (const auto &p : db.patients) {
            int count = 0;
            for (const auto &a : db.appointments)
                if (a.patientId == p->id() && a.status == AppointmentStatus::Completed &&
                    included(a))
                    ++count;
            if (count)
                rows.push_back({p->id(), p->details().name,
                                db.archivedPatients.contains(p->id()) ? "Archived" : "Active",
                                count});
            visits += count;
        }
        algorithms::mergeSort(rows,
                              [](const Count &a, const Count &b) { return a.count > b.count; });
        for (const auto &r : rows)
            report.rows.append({r.id, r.name, QString::number(r.count), r.state});
        report.summary = QString("%1 completed visits across %2 patients | %3")
                             .arg(visits)
                             .arg(rows.size())
                             .arg(range);
    } else if (type == ReportType::Pending) {
        report.title = "Patients with pending appointments";
        report.headers = {"Patient ID", "Patient",       "Appointment",
                          "Service",    "Scheduled for", "Status"};
        for (const auto &a : db.appointments)
            if (isPending(a.status) && included(a))
                report.rows.append({a.patientId, db.patient(a.patientId)->details().name, a.id,
                                    a.service, a.start.toLocalTime().toString("yyyy-MM-dd HH:mm"),
                                    statusName(a.status)});
        report.summary = QString("%1 pending visits | %2").arg(report.rows.size()).arg(range);
    } else if (type == ReportType::Categories) {
        report.title = "Patient category distribution";
        report.headers = {"Category", "Active patients", "Archived patients", "Total"};
        for (auto category :
             {PatientCategory::General, PatientCategory::Emergency, PatientCategory::FollowUp}) {
            int active = 0, archived = 0;
            for (const auto &p : db.patients)
                if (p->category() == category) {
                    if (db.archivedPatients.contains(p->id()))
                        ++archived;
                    else
                        ++active;
                }
            report.rows.append({categoryName(category), QString::number(active),
                                QString::number(archived), QString::number(active + archived)});
        }
        report.summary = QString("%1 registered records | Category counts are current, independent "
                                 "of the date range.")
                             .arg(db.patients.size());
    } else {
        report.title = "Appointment status summary";
        report.headers = {"Status", "Appointments"};
        int total = 0, scheduled = 0, checked = 0, consulting = 0, completed = 0, cancelled = 0,
            noShow = 0;
        for (const auto &a : db.appointments)
            if (included(a)) {
                ++total;
                switch (a.status) {
                case AppointmentStatus::Scheduled:
                    ++scheduled;
                    break;
                case AppointmentStatus::CheckedIn:
                    ++checked;
                    break;
                case AppointmentStatus::InConsultation:
                    ++consulting;
                    break;
                case AppointmentStatus::Completed:
                    ++completed;
                    break;
                case AppointmentStatus::NoShow:
                    ++noShow;
                    break;
                case AppointmentStatus::Cancelled:
                    ++cancelled;
                    break;
                }
            }
        report.rows = {{"Total", QString::number(total)},
                       {"Completed", QString::number(completed)},
                       {"Scheduled", QString::number(scheduled)},
                       {"Checked in", QString::number(checked)},
                       {"In consultation", QString::number(consulting)},
                       {"Cancelled", QString::number(cancelled)},
                       {"No-show", QString::number(noShow)}};
        report.summary = QString("%1 visits | %2").arg(total).arg(range);
    }
    return report;
}
QString reportCsv(const Report &report) {
    auto encode = [](QString value) {
        const auto trimmed = value.trimmed();
        if (!trimmed.isEmpty() && QString("=+-@").contains(trimmed.front()))
            value.prepend("'");
        value.replace("\"", "\"\"");
        return "\"" + value + "\"";
    };
    QString text;
    auto append = [&](const QStringList &row) {
        QStringList cells;
        for (const auto &c : row)
            cells.append(encode(c));
        text += cells.join(",") + "\r\n";
    };
    append(report.headers);
    for (const auto &row : report.rows)
        append(row);
    append({"Summary", report.summary});
    return text;
}
QString reportHtml(const Report &report) {
    QString html = "<h1>" + report.title.toHtmlEscaped() + "</h1><p>" +
                   report.summary.toHtmlEscaped() +
                   "</p><table border='1' cellspacing='0' cellpadding='6'><tr>";
    for (const auto &h : report.headers)
        html += "<th>" + h.toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (const auto &row : report.rows) {
        html += "<tr>";
        for (const auto &c : row)
            html += "<td>" + c.toHtmlEscaped() + "</td>";
        html += "</tr>";
    }
    return html + "</table><p>SCAPMS - generated " +
           QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "</p>";
}
} // namespace clinic::controller

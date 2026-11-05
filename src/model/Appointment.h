#pragma once
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace clinic::model {
enum class AppointmentStatus { Scheduled, Completed, Cancelled, CheckedIn, InConsultation, NoShow };
inline QString statusName(AppointmentStatus status) {
    switch (status) {
    case AppointmentStatus::NoShow:
        return "No-show";
    case AppointmentStatus::CheckedIn:
        return "Checked in";
    case AppointmentStatus::InConsultation:
        return "In consultation";
    case AppointmentStatus::Scheduled:
        return "Scheduled";
    case AppointmentStatus::Completed:
        return "Completed";
    case AppointmentStatus::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}
inline bool isPending(AppointmentStatus s) {
    return s == AppointmentStatus::Scheduled || s == AppointmentStatus::CheckedIn ||
           s == AppointmentStatus::InConsultation;
}
struct Appointment {
    QString id;
    QString patientId;
    QString service;
    QDateTime start;
    int durationMinutes = 30;
    AppointmentStatus status = AppointmentStatus::Scheduled;
    QString consultationNotes;
    QString changeReason, clinician, room;
    QStringList corrections;
    bool walkIn = false;
    QDateTime consultationStarted, completedAt;
    QDateTime end() const {
        return start.addSecs(durationMinutes * 60);
    }
};
struct QueueEntry {
    QString patientId;
    QDateTime arrivedAt;
    QString appointmentId;
};
} // namespace clinic::model

#include "FileHandler.h"
#include "model/Errors.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cmath>
#include <limits>

namespace clinic::persistence {
using namespace model;
namespace {
QString stringField(const QJsonObject &object, const QString &key) {
    if (!object.value(key).isString())
        throw FileError("Missing or invalid text field: " + key);
    return object.value(key).toString();
}
int integerField(const QJsonObject &object, const QString &key) {
    const auto value = object.value(key);
    if (!value.isDouble())
        throw FileError("Missing or invalid integer field: " + key);
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
        number > std::numeric_limits<int>::max())
        throw FileError("Invalid integer field: " + key);
    return static_cast<int>(number);
}
QJsonArray arrayField(const QJsonObject &object, const QString &key) {
    if (!object.value(key).isArray())
        throw FileError("Missing or invalid list: " + key);
    return object.value(key).toArray();
}
QJsonObject objectValue(const QJsonValue &value) {
    if (!value.isObject())
        throw FileError("Expected an object in the data file.");
    return value.toObject();
}
} // namespace
QJsonObject FileHandler::serialize(const ClinicDatabase &db) {
    QJsonArray patients, appointments, waiting, archived, users, audit;
    for (const auto &id : db.archivedPatients)
        archived.append(id);
    for (const auto &u : db.users)
        users.append(QJsonObject{{"username", u.username},
                                 {"role", security::roleName(u.role)},
                                 {"salt", QString::fromLatin1(u.salt.toBase64())},
                                 {"passwordHash", QString::fromLatin1(u.passwordHash.toBase64())},
                                 {"iterations", u.iterations},
                                 {"active", u.active}});
    for (const auto &a : db.audit)
        audit.append(QJsonObject{{"time", a.time.toString(Qt::ISODateWithMs)},
                                 {"actor", a.actor},
                                 {"action", a.action}});
    for (const auto &p : db.patients) {
        const auto &d = p->details();
        patients.append(QJsonObject{{"id", d.id},
                                    {"name", d.name},
                                    {"age", d.age},
                                    {"contact", d.contact},
                                    {"category", categoryName(p->category())},
                                    {"reason", d.reason},
                                    {"urgency", d.urgency},
                                    {"previousVisit", d.previousVisit}});
    }
    for (const auto &a : db.appointments)
        appointments.append(
            QJsonObject{{"id", a.id},
                        {"patientId", a.patientId},
                        {"service", a.service},
                        {"start", a.start.toUTC().toString(Qt::ISODateWithMs)},
                        {"durationMinutes", a.durationMinutes},
                        {"status", statusName(a.status)},
                        {"consultationNotes", a.consultationNotes},
                        {"changeReason", a.changeReason},
                        {"clinician", a.clinician},
                        {"room", a.room},
                        {"walkIn", a.walkIn},
                        {"corrections", QJsonArray::fromStringList(a.corrections)},
                        {"consultationStarted", a.consultationStarted.toString(Qt::ISODateWithMs)},
                        {"completedAt", a.completedAt.toString(Qt::ISODateWithMs)}});
    auto copy = db.waiting;
    while (!copy.empty()) {
        waiting.append(
            QJsonObject{{"patientId", copy.front().patientId},
                        {"arrivedAt", copy.front().arrivedAt.toUTC().toString(Qt::ISODateWithMs)},
                        {"appointmentId", copy.front().appointmentId}});
        copy.pop();
    }
    return {{"schemaVersion", 3},
            {"clinicians", QJsonArray::fromStringList(db.clinicians)},
            {"rooms", QJsonArray::fromStringList(db.rooms)},
            {"archivedPatients", archived},
            {"users", users},
            {"audit", audit},
            {"nextPatientId", db.nextPatientId},
            {"nextAppointmentId", db.nextAppointmentId},
            {"patients", patients},
            {"appointments", appointments},
            {"waitingQueue", waiting}};
}
ClinicDatabase FileHandler::deserialize(const QJsonObject &root) {
    const auto version = integerField(root, "schemaVersion");
    if (version < 1 || version > 3)
        throw FileError("Unsupported data schema version.");
    ClinicDatabase db;
    if (version >= 3) {
        for (auto v : arrayField(root, "clinicians")) {
            if (!v.isString())
                throw FileError("Invalid clinician.");
            db.clinicians << v.toString();
        }
        for (auto v : arrayField(root, "rooms")) {
            if (!v.isString())
                throw FileError("Invalid room.");
            db.rooms << v.toString();
        }
    }
    db.nextPatientId = integerField(root, "nextPatientId");
    db.nextAppointmentId = integerField(root, "nextAppointmentId");
    for (const auto value : arrayField(root, "patients")) {
        const auto object = objectValue(value);
        PatientDetails d;
        d.id = stringField(object, "id");
        d.name = stringField(object, "name");
        d.age = integerField(object, "age");
        d.contact = stringField(object, "contact");
        d.reason = stringField(object, "reason");
        d.urgency = integerField(object, "urgency");
        d.previousVisit = stringField(object, "previousVisit");
        db.patients.push_back(makePatient(parseCategory(stringField(object, "category")), d));
    }
    for (const auto value : arrayField(root, "appointments")) {
        const auto object = objectValue(value);
        Appointment a;
        a.id = stringField(object, "id");
        a.patientId = stringField(object, "patientId");
        a.service = stringField(object, "service");
        a.start =
            QDateTime::fromString(stringField(object, "start"), Qt::ISODateWithMs).toLocalTime();
        a.durationMinutes = integerField(object, "durationMinutes");
        a.consultationNotes = stringField(object, "consultationNotes");
        if (version >= 3) {
            a.changeReason = stringField(object, "changeReason");
            a.clinician = stringField(object, "clinician");
            a.room = stringField(object, "room");
            if (!object.value("walkIn").isBool())
                throw FileError("Invalid walk-in flag.");
            a.walkIn = object.value("walkIn").toBool();
            for (auto v : arrayField(object, "corrections")) {
                if (!v.isString())
                    throw FileError("Invalid note correction.");
                a.corrections << v.toString();
            }
            a.consultationStarted = QDateTime::fromString(
                stringField(object, "consultationStarted"), Qt::ISODateWithMs);
            a.completedAt =
                QDateTime::fromString(stringField(object, "completedAt"), Qt::ISODateWithMs);
        }
        const auto status = stringField(object, "status");
        if (status == "Scheduled")
            a.status = AppointmentStatus::Scheduled;
        else if (status == "Checked in")
            a.status = AppointmentStatus::CheckedIn;
        else if (status == "In consultation")
            a.status = AppointmentStatus::InConsultation;
        else if (status == "Completed")
            a.status = AppointmentStatus::Completed;
        else if (status == "No-show")
            a.status = AppointmentStatus::NoShow;
        else if (status == "Cancelled")
            a.status = AppointmentStatus::Cancelled;
        else
            throw FileError("Unknown appointment status: " + status);
        db.appointments.push_back(a);
    }
    for (const auto value : arrayField(root, "waitingQueue")) {
        const auto object = objectValue(value);
        db.waiting.push({stringField(object, "patientId"),
                         QDateTime::fromString(stringField(object, "arrivedAt"), Qt::ISODateWithMs)
                             .toLocalTime(),
                         object.value("appointmentId").toString()});
    }
    if (version >= 2) {
        for (const auto value : arrayField(root, "archivedPatients")) {
            if (!value.isString())
                throw FileError("Invalid archived ID.");
            db.archivedPatients.insert(value.toString());
        }
        for (const auto value : arrayField(root, "users")) {
            const auto o = objectValue(value);
            security::UserAccount u;
            u.username = stringField(o, "username");
            u.role = security::parseRole(stringField(o, "role"));
            u.salt = QByteArray::fromBase64(stringField(o, "salt").toLatin1());
            u.passwordHash = QByteArray::fromBase64(stringField(o, "passwordHash").toLatin1());
            u.iterations = integerField(o, "iterations");
            if (!o.value("active").isBool())
                throw FileError("Invalid account status.");
            u.active = o.value("active").toBool();
            db.users.push_back(u);
        }
        for (const auto value : arrayField(root, "audit")) {
            const auto o = objectValue(value);
            auto when = QDateTime::fromString(stringField(o, "time"), Qt::ISODateWithMs);
            if (!when.isValid())
                throw FileError("Invalid audit timestamp.");
            db.audit.push_back({when, stringField(o, "actor"), stringField(o, "action")});
        }
    }
    db.validate();
    return db;
}
ClinicDatabase FileHandler::load(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw FileError("Could not open file: " + file.errorString());
    if (file.size() > 20 * 1024 * 1024)
        throw FileError("Data files must be smaller than 20 MB.");
    QJsonParseError error;
    auto bytes = file.readAll();
    if (bytes.startsWith("SCAPMS-PROTECTED-2\n"))
        bytes = security::unprotect(bytes.mid(QByteArray("SCAPMS-PROTECTED-2\n").size()));
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw FileError("Invalid JSON data: " + error.errorString());
    return deserialize(document.object());
}
void FileHandler::save(const ClinicDatabase &db, const QString &path) {
    db.validate();
    const bool encrypted = QFileInfo(path).suffix().toLower() != "json";
    if (!encrypted && !db.users.empty())
        throw FileError("Staff-protected workspaces must be saved as .clinic files.");
    auto bytes = QJsonDocument(serialize(db)).toJson(QJsonDocument::Indented);
    if (encrypted)
        bytes = QByteArray("SCAPMS-PROTECTED-2\n") + security::protect(bytes);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw FileError("Could not save workspace: " + file.errorString());
}
void FileHandler::writeText(const QString &path, const QString &text) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        throw FileError("Could not save file: " + file.errorString());
    const auto bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
        throw FileError("Save failed: " + file.errorString());
}
} // namespace clinic::persistence

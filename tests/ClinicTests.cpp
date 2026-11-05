#include "algorithms/Algorithms.h"
#include "algorithms/Benchmark.h"
#include "security/PortableBackup.h"
#include "persistence/WorkspaceGuard.h"
#include "gui/PatientTableModel.h"
#include "controller/ClinicManager.h"
#include "controller/ReportService.h"
#include "gui/MainWindow.h"
#include "gui/PatientDialog.h"
#include "gui/panels/PatientsPanel.h"
#include "model/EmergencyPatient.h"
#include "model/Errors.h"
#include "model/FollowUpPatient.h"
#include "persistence/FileHandler.h"
#include "persistence/RecoveryService.h"
#include "security/AccessControl.h"
#include <QtTest>
#include <QtWidgets>
#include <random>
using namespace clinic;
using namespace clinic::model;
using namespace clinic::controller;
using namespace clinic::algorithms;
using persistence::FileHandler;
namespace {
PatientDetails detail(QString id, QString name = "Test Patient", int age = 25) {
    return {id,
            name,
            age,
            (id.isEmpty() ? QString("test") : id.toLower()) + "@example.test",
            "Health review",
            4,
            "VIS-001"};
}
QJsonObject stateOnly(const ClinicDatabase &db) {
    auto json = FileHandler::serialize(db);
    json.remove("audit");
    return json;
}
ClinicDatabase fixture() {
    ClinicDatabase db;
    db.patients.push_back(makePatient(PatientCategory::General, detail("P1", "Ada", 23)));
    db.patients.push_back(makePatient(PatientCategory::Emergency, detail("P2", "Tunde", 31)));
    db.patients.push_back(makePatient(PatientCategory::FollowUp, detail("P3", "Zainab", 27)));
    const auto now = QDateTime::currentDateTime();
    db.appointments.push_back(
        {"A1", "P1", "Review", now.addSecs(-3600), 30, AppointmentStatus::Scheduled, {}});
    db.appointments.push_back({"A2", "P2", "Consultation", now.addSecs(-7200), 30,
                               AppointmentStatus::Completed, "Reviewed"});
    db.appointments.push_back(
        {"A3", "P1", "Consultation", now.addSecs(7200), 30, AppointmentStatus::Cancelled, {}});
    db.validate();
    return db;
}
} // namespace
class ClinicTests : public QObject {
    Q_OBJECT
  private slots:
    void patientValidationAndPolymorphism() {
        QVERIFY_EXCEPTION_THROWN(makePatient(PatientCategory::General, detail("P1", "", 25)),
                                 ValidationError);
        QVERIFY_EXCEPTION_THROWN(makePatient(PatientCategory::General, detail("P1", "Ada", 121)),
                                 ValidationError);
        auto d = detail("P1");
        d.previousVisit.clear();
        QVERIFY_EXCEPTION_THROWN(makePatient(PatientCategory::FollowUp, d), ValidationError);
        auto p = makePatient(PatientCategory::Emergency, detail("E1"));
        QVERIFY(processPatient(*p).contains("level 4"));
        QVERIFY(dynamic_cast<EmergencyPatient *>(p->clone().get()) != nullptr);
        auto f = makePatient(PatientCategory::FollowUp, detail("F1"));
        QVERIFY(processPatient(*f).contains("VIS-001"));
    }
    void duplicateAndFailedOperationAreAtomic() {
        ClinicManager m;
        m.addPatient(PatientCategory::General, detail("P1"));
        const auto before = stateOnly(m.database());
        QVERIFY_EXCEPTION_THROWN(m.addPatient(PatientCategory::General, detail("P1")),
                                 ValidationError);
        QCOMPARE(stateOnly(m.database()), before);
        m.undo();
        QCOMPARE(m.database().patients.size(), size_t(0));
        QVERIFY(!m.canUndo());
    }
    void cascadeDeleteUndoRestoresSubtypeAndQueue() {
        ClinicManager m;
        m.replace(fixture());
        m.enqueue("P2");
        m.enqueue("P1");
        m.access("P2");
        const auto before = stateOnly(m.database());
        m.deletePatient("P2");
        QVERIFY(m.database().archivedPatients.contains("P2"));
        QVERIFY(m.database().appointment("A2"));
        QCOMPARE(m.database().waiting.front().patientId, QString("P1"));
        m.undo();
        QCOMPARE(stateOnly(m.database()), before);
        QVERIFY(dynamic_cast<const EmergencyPatient *>(m.database().patient("P2")));
        QCOMPARE(m.database().waiting.front().patientId, QString("P2"));
        QVERIFY(!m.database().frequent[0].patientId.isEmpty());
    }
    void editUndoPreservesRelationships() {
        ClinicManager m;
        m.replace(fixture());
        m.enqueue("P1");
        m.editPatient("P1", PatientCategory::FollowUp, detail("OTHER", "Updated", 55));
        QCOMPARE(m.database().patient("P1")->details().name, QString("Updated"));
        QCOMPARE(m.database().appointment("A1")->patientId, QString("P1"));
        QCOMPARE(m.database().waiting.front().patientId, QString("P1"));
        m.undo();
        QCOMPARE(m.database().patient("P1")->details().name, QString("Ada"));
        QCOMPARE(m.database().patient("P1")->category(), PatientCategory::General);
    }
    void fifoAndDuplicateQueue() {
        ClinicManager m;
        m.replace(fixture());
        QVERIFY_EXCEPTION_THROWN(m.callNext(), ValidationError);
        m.enqueue("P3");
        m.enqueue("P1");
        QVERIFY_EXCEPTION_THROWN(m.enqueue("P3"), ValidationError);
        QVERIFY(m.callNext().startsWith("Zainab"));
        QCOMPARE(m.database().waiting.front().patientId, QString("P1"));
        m.undo();
        QCOMPARE(m.database().waiting.front().patientId, QString("P3"));
        m.removeFromQueue("P3");
        QCOMPARE(m.database().waiting.front().patientId, QString("P1"));
    }
    void appointmentLifecycleAndConflicts() {
        ClinicManager m;
        m.replace(fixture());
        const auto future = QDateTime::currentDateTime().addDays(1);
        const auto id = m.schedule("P1", "Review", future, 30);
        QVERIFY_EXCEPTION_THROWN(m.schedule("P1", "Overlap", future.addSecs(60), 30),
                                 ValidationError);
        const auto adjacent = m.schedule("P1", "Next slot", future.addSecs(1800), 30);
        QVERIFY(!adjacent.isEmpty());
        QVERIFY_EXCEPTION_THROWN(m.complete(id, "Notes"), ValidationError);
        m.cancel(id, "Patient requested cancellation");
        QCOMPARE(m.database().appointment(id)->status, AppointmentStatus::Cancelled);
        m.undo();
        QCOMPARE(m.database().appointment(id)->status, AppointmentStatus::Scheduled);
        m.checkIn("A1");
        m.callNext();
        m.complete("A1", "Review complete");
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::Completed);
        QVERIFY(m.database().waiting.empty());
        QVERIFY_EXCEPTION_THROWN(m.cancel("A1"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.schedule("missing", "Review", future, 30), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.schedule("P1", "Review", future, 0), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.schedule("P1", "Review", future.addDays(-2), 30),
                                 ValidationError);
    }
    void manualAlgorithmsEdgeCasesAndStability() {
        for (auto algorithm : {SortAlgorithm::Insertion, SortAlgorithm::Merge}) {
            PatientView empty;
            sortPatients(empty, SortField::Id, algorithm);
            QVERIFY(!binarySearchById(empty, "P1"));
            ClinicDatabase db;
            db.patients.push_back(makePatient(PatientCategory::General, detail("P3", "same", 30)));
            db.patients.push_back(makePatient(PatientCategory::General, detail("P1", "SAME", 10)));
            db.patients.push_back(makePatient(PatientCategory::General, detail("P2", "Alpha", 20)));
            auto view = viewOf(db.patients);
            sortPatients(view, SortField::Name, algorithm);
            QCOMPARE(view[0]->id(), QString("P2"));
            QCOMPARE(view[1]->id(), QString("P3"));
            QCOMPARE(view[2]->id(), QString("P1"));
            sortPatients(view, SortField::Age, algorithm);
            QCOMPARE(view[0]->details().age, 10);
            QCOMPARE(view[2]->details().age, 30);
            sortPatients(view, SortField::Id, algorithm);
            QCOMPARE(binarySearchById(view, "P1")->id(), QString("P1"));
            QCOMPARE(binarySearchById(view, "P3")->id(), QString("P3"));
            QVERIFY(!binarySearchById(view, "P9"));
            QVERIFY(!binarySearchById(view, "P0"));
            QCOMPARE(linearSearchById(db.patients, "P2")->id(), QString("P2"));
            QCOMPARE(filterPatients(view, "same", false).size(), size_t(2));
            QCOMPARE(filterPatients(view, "general", true).size(), size_t(3));
        }
        std::mt19937 random(42);
        std::vector<int> numbers;
        for (int i = 0; i < 1000; ++i)
            numbers.push_back(static_cast<int>(random() % 50));
        auto copy = numbers;
        Metrics metrics;
        mergeSort(numbers, [](int a, int b) { return a < b; }, &metrics);
        insertionSort(copy, [](int a, int b) { return a < b; });
        QCOMPARE(numbers, copy);
        QVERIFY(metrics.maxRecursionDepth > 1);
        for (size_t i = 1; i < numbers.size(); ++i)
            QVERIFY(numbers[i - 1] <= numbers[i]);
    }
    void persistenceRoundTripAndMalformedData() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto db = fixture();
        db.waiting.push({"P3", QDateTime::currentDateTime()});
        const auto path = dir.filePath("clinic.json");
        FileHandler::save(db, path);
        auto loaded = FileHandler::load(path);
        QCOMPARE(FileHandler::serialize(db), FileHandler::serialize(loaded));
        QVERIFY(dynamic_cast<const FollowUpPatient *>(loaded.patient("P3")));
        auto invalid = FileHandler::serialize(db);
        invalid["schemaVersion"] = 99;
        QVERIFY_EXCEPTION_THROWN(FileHandler::deserialize(invalid), FileError);
        invalid = FileHandler::serialize(db);
        auto patients = invalid["patients"].toArray();
        patients.append(patients[0]);
        invalid["patients"] = patients;
        QVERIFY_EXCEPTION_THROWN(FileHandler::deserialize(invalid), ValidationError);
        invalid = FileHandler::serialize(db);
        auto appointments = invalid["appointments"].toArray();
        auto a = appointments[0].toObject();
        a["patientId"] = "missing";
        appointments[0] = a;
        invalid["appointments"] = appointments;
        QVERIFY_EXCEPTION_THROWN(FileHandler::deserialize(invalid), ValidationError);
        FileHandler::writeText(path, "{invalid json");
        QVERIFY_EXCEPTION_THROWN(FileHandler::load(path), FileError);
        QVERIFY_EXCEPTION_THROWN(FileHandler::save(db, dir.filePath("missing/folder/data.json")),
                                 FileError);
        ClinicManager m;
        m.replace(fixture());
        const auto before = FileHandler::serialize(m.database());
        try {
            m.replace(FileHandler::load(path));
        } catch (const ClinicError &) {
        }
        QCOMPARE(FileHandler::serialize(m.database()), before);
    }
    void idGenerationAfterLoadAndCacheCapacity() {
        ClinicManager m;
        m.addPatient(PatientCategory::General, detail("P0001"));
        auto blank = detail("");
        m.addPatient(PatientCategory::General, blank);
        QVERIFY(m.database().patient("P0002"));
        m.replace(FileHandler::deserialize(FileHandler::serialize(m.database())));
        m.addPatient(PatientCategory::General, blank);
        QVERIFY(m.database().patient("P0003"));
        for (int i = 4; i <= 12; ++i)
            m.addPatient(PatientCategory::General, detail("X" + QString::number(i)));
        m.access("P0001");
        m.access("P0001");
        for (const auto &p : m.database().patients)
            m.access(p->id());
        size_t count = 0;
        bool retained = false;
        for (const auto &entry : m.database().frequent) {
            if (!entry.patientId.isEmpty())
                ++count;
            if (entry.patientId == "P0001")
                retained = true;
        }
        QCOMPARE(count, size_t(10));
        QVERIFY(retained);
    }
    void reportsAndCsv() {
        auto db = fixture();
        auto pending = generateReport(db, ReportType::Pending);
        QCOMPARE(pending.rows.size(), 1);
        auto attendance = generateReport(db, ReportType::Attendance);
        QCOMPARE(attendance.rows[0][0], QString("P2"));
        auto categories = generateReport(db, ReportType::Categories);
        QCOMPARE(categories.rows.size(), 3);
        for (const auto &row : categories.rows)
            QCOMPARE(row[1], QString("1"));
        auto daily = generateReport(db, ReportType::Daily, db.appointments[0].start.date());
        int expected = 0;
        for (const auto &a : db.appointments)
            if (a.start.date() == db.appointments[0].start.date())
                ++expected;
        QCOMPARE(daily.rows[0][1].toInt(), expected);
        Report r{"CSV", {"Name"}, {{"A, \"B\""}, {"=SUM(A1:A2)"}}};
        QVERIFY(reportCsv(r).contains("\"A, \"\"B\"\"\""));
        QVERIFY(reportCsv(r).contains("'=SUM"));
    }
    void guiRegistrationSearchSortQueueAndSave() {
        QTemporaryDir dir;
        gui::MainWindow window;
        window.show();
        QApplication::processEvents();
        auto *nav = window.findChild<QListWidget *>("navigation");
        QVERIFY(nav);
        nav->setCurrentRow(1);
        QTimer::singleShot(30, [&] {
            auto *dialog = window.findChild<QDialog *>("patientDialog");
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("patientName")->setText("GUI Patient");
            dialog->findChild<QLineEdit *>("patientContact")->setText("gui@example.test");
            dialog->findChild<QLineEdit *>("patientReason")->setText("Routine review");
            QTest::mouseClick(dialog->findChild<QDialogButtonBox *>("patientButtons")
                                  ->button(QDialogButtonBox::Save),
                              Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton *>("addPatient"), Qt::LeftButton);
        QCOMPARE(window.manager().database().patients.size(), size_t(1));
        nav->setCurrentRow(4);
        window.findChild<QComboBox *>("searchAlgorithm")->setCurrentIndex(1);
        window.findChild<QLineEdit *>("searchText")->setText("P0001");
        QTest::mouseClick(window.findChild<QPushButton *>("searchButton"), Qt::LeftButton);
        QCOMPARE(window.findChild<QTableView *>("searchResults")->model()->rowCount(), 1);
        window.findChild<QComboBox *>("sortAlgorithm")->setCurrentIndex(1);
        QTest::mouseClick(window.findChild<QPushButton *>("sortButton"), Qt::LeftButton);
        QVERIFY(window.findChild<QLabel *>("searchInfo")->text().contains("Recursive merge"));
        nav->setCurrentRow(3);
        QTest::mouseClick(window.findChild<QPushButton *>("enqueuePatient"), Qt::LeftButton);
        QCOMPARE(window.manager().database().waiting.size(), size_t(1));
        QTest::mouseClick(window.findChild<QPushButton *>("callNext"), Qt::LeftButton);
        QVERIFY(window.manager().database().waiting.empty());
        QTest::mouseClick(window.findChild<QPushButton *>("undoButton"), Qt::LeftButton);
        QCOMPARE(window.manager().database().waiting.size(), size_t(1));
        const auto file = dir.filePath("gui.json");
        FileHandler::save(window.manager().database(), file);
        window.loadPath(file);
        QVERIFY(!window.manager().dirty());
        nav->setCurrentRow(5);
        for (int i = 0; i < 4; ++i)
            window.findChild<QComboBox *>("reportType")->setCurrentIndex(i);
        window.close();
    }
    void guiAppointmentDeleteUndoAndSave() {
        QTemporaryDir dir;
        gui::MainWindow window;
        const auto path = dir.filePath("clinic.json");
        FileHandler::save(fixture(), path);
        window.loadPath(path);
        window.show();
        auto *nav = window.findChild<QListWidget *>("navigation");
        nav->setCurrentRow(2);
        auto *appointments = window.findChild<QTableWidget *>("appointmentsTable");
        window.findChild<QDateTimeEdit *>("appointmentTime")
            ->setDateTime(QDateTime::currentDateTime().addDays(2));
        QTest::mouseClick(window.findChild<QPushButton *>("scheduleAppointment"), Qt::LeftButton);
        QCOMPARE(window.manager().database().appointments.size(), size_t(4));
        appointments->selectRow(3);
        QTimer::singleShot(30, [&] {
            auto *input = window.findChild<QInputDialog *>();
            QVERIFY(input);
            input->setTextValue("Patient requested cancellation");
            input->accept();
        });
        QTest::mouseClick(window.findChild<QPushButton *>("cancelAppointment"), Qt::LeftButton);
        QCOMPARE(window.manager().database().appointments[3].status, AppointmentStatus::Cancelled);
        QTest::mouseClick(window.findChild<QPushButton *>("undoButton"), Qt::LeftButton);
        QCOMPARE(window.manager().database().appointments[3].status, AppointmentStatus::Scheduled);
        window.manager().checkIn("A1");
        window.manager().callNext();
        window.refresh();
        appointments->selectRow(0);
        QTimer::singleShot(30, [&] {
            auto *input = window.findChild<QInputDialog *>();
            QVERIFY(input);
            input->setTextValue("GUI consultation completed");
            input->accept();
        });
        QTest::mouseClick(window.findChild<QPushButton *>("completeAppointment"), Qt::LeftButton);
        QCOMPARE(window.manager().database().appointment("A1")->status,
                 AppointmentStatus::Completed);
        nav->setCurrentRow(1);
        window.findChild<QTableView *>("patientsTable")->selectRow(1);
        QTimer::singleShot(30, [&] {
            auto *box = window.findChild<QMessageBox *>();
            QVERIFY(box);
            box->button(QMessageBox::Yes)->click();
        });
        QTest::mouseClick(window.findChild<QPushButton *>("deletePatient"), Qt::LeftButton);
        QVERIFY(window.manager().database().archivedPatients.contains("P2"));
        QTest::mouseClick(window.findChild<QPushButton *>("undoButton"), Qt::LeftButton);
        QVERIFY(!window.manager().database().archivedPatients.contains("P2"));
        QTest::mouseClick(window.findChild<QPushButton *>("saveButton"), Qt::LeftButton);
        QVERIFY(!window.manager().dirty());
        QCOMPARE(FileHandler::serialize(FileHandler::load(path)),
                 FileHandler::serialize(window.manager().database()));
        window.close();
    }

    void rescheduleConflictAndUndo() {
        ClinicManager m;
        m.replace(fixture());
        const auto future = QDateTime::currentDateTime().addDays(1);
        const auto first = m.schedule("P1", "Review", future, 30);
        const auto second = m.schedule("P1", "Review", future.addSecs(3600), 30);
        const auto before = stateOnly(m.database());
        QVERIFY_EXCEPTION_THROWN(m.reschedule(second, future.addSecs(60), 30), ValidationError);
        QCOMPARE(stateOnly(m.database()), before);
        m.reschedule(second, future.addSecs(7200), 45);
        QCOMPARE(m.database().appointment(second)->durationMinutes, 45);
        m.undo();
        QCOMPARE(stateOnly(m.database()), before);
        m.cancel(first, "Changed plans");
        QVERIFY_EXCEPTION_THROWN(m.reschedule(first, future.addSecs(9000), 30), ValidationError);
    }
    void linkedCheckInConsultationLifecycle() {
        ClinicManager m;
        auto db = fixture();
        db.appointments[0].start = QDateTime::currentDateTime().addSecs(-60);
        m.replace(db);
        m.checkIn("A1");
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::CheckedIn);
        QCOMPARE(m.database().waiting.front().appointmentId, QString("A1"));
        QVERIFY_EXCEPTION_THROWN(m.checkIn("A1"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.enqueue("P1"), ValidationError);
        QVERIFY(m.callNext().contains("Ada"));
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::InConsultation);
        m.complete("A1", "Linked workflow complete");
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::Completed);
        m.undo();
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::InConsultation);
        m.undo();
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::CheckedIn);
        m.removeFromQueue("P1");
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::Scheduled);
        m.checkIn("A1");
        m.cancel("A1", "Cannot attend");
        QVERIFY(m.database().waiting.empty());
    }
    void archiveRetainsHistoryAndAudit() {
        ClinicManager m;
        m.replace(fixture());
        const auto oldCount = m.database().appointments.size();
        m.deletePatient("P1");
        QVERIFY(m.database().archivedPatients.contains("P1"));
        QCOMPARE(m.database().appointments.size(), oldCount);
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::Cancelled);
        QVERIFY_EXCEPTION_THROWN(m.enqueue("P1"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(
            m.schedule("P1", "Visit", QDateTime::currentDateTime().addDays(1), 30),
            ValidationError);
        const auto auditSize = m.database().audit.size();
        m.undo();
        QVERIFY(!m.database().archivedPatients.contains("P1"));
        QCOMPARE(m.database().audit.size(), auditSize + 1);
        QVERIFY(m.database().audit.back().action.startsWith("Undo:"));
        m.deletePatient("P1");
        m.restorePatient("P1");
        QVERIFY(!m.database().archivedPatients.contains("P1"));
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::Cancelled);
    }
    void staffRolesPasswordsAndLocking() {
        ClinicManager m;
        m.replace(fixture());
        m.configureAdministrator("admin", "a long admin passphrase");
        m.addUser("desk", "a long desk passphrase", security::Role::Receptionist);
        m.addUser("doctor", "a long doctor passphrase", security::Role::Clinician);
        m.addUser("reader", "a long reader passphrase", security::Role::Viewer);
        const auto secured = m.database();
        QVERIFY(!secured.users[0].passwordHash.contains("passphrase"));
        m.lock();
        QVERIFY(!m.canEdit());
        QVERIFY_EXCEPTION_THROWN(m.addPatient(PatientCategory::General, detail("P9")),
                                 ValidationError);
        m.login("desk", "a long desk passphrase");
        QVERIFY(m.canEdit());
        QVERIFY(!m.canViewClinicalNotes());
        QVERIFY_EXCEPTION_THROWN(m.complete("A1", "Notes"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.deletePatient("P1"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(
            m.addUser("other", "a long other password", security::Role::Viewer), ValidationError);
        m.addPatient(PatientCategory::General, detail("P9"));
        m.lock();
        m.login("reader", "a long reader passphrase");
        QVERIFY_EXCEPTION_THROWN(m.enqueue("P1"), ValidationError);
        QVERIFY(!m.canUndo());
        m.lock();
        m.login("doctor", "a long doctor passphrase");
        m.checkIn("A1");
        m.callNext();
        m.complete("A1", "Clinician completed");
        m.lock();
        m.login("admin", "a long admin passphrase");
        m.setUserActive("reader", false);
        QVERIFY_EXCEPTION_THROWN(m.setUserActive("admin", false), ValidationError);
        m.changePassword("a long admin passphrase", "a different long admin password");
        m.lock();
        QVERIFY_EXCEPTION_THROWN(m.login("reader", "a long reader passphrase"), ValidationError);
        m.login("admin", "a different long admin password");
        ClinicManager target;
        target.replace(fixture());
        const auto before = stateOnly(target.database());
        QVERIFY_EXCEPTION_THROWN(target.replace(secured, "admin", "wrong password"),
                                 ValidationError);
        QCOMPARE(stateOnly(target.database()), before);
        target.replace(secured, "admin", "a long admin passphrase");
        QVERIFY(target.isAdmin());
    }
    void protectedStorageTamperAndRecovery() {
#ifdef Q_OS_WIN
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ClinicManager m;
        m.replace(fixture());
        m.configureAdministrator("admin", "a long admin passphrase");
        const auto file = dir.filePath("secure.clinic");
        persistence::RecoveryService::saveWithBackup(m.database(), file);
        QFile contents(file);
        QVERIFY(contents.open(QIODevice::ReadOnly));
        auto bytes = contents.readAll();
        contents.close();
        QVERIFY(bytes.startsWith("SCAPMS-PROTECTED-2\n"));
        QVERIFY(!bytes.contains("Ada"));
        QVERIFY(!bytes.contains("passphrase"));
        auto restored = FileHandler::load(file);
        QCOMPARE(FileHandler::serialize(restored), FileHandler::serialize(m.database()));
        QVERIFY_EXCEPTION_THROWN(FileHandler::save(m.database(), dir.filePath("leak.json")),
                                 FileError);
        for (int i = 0; i < 7; ++i) {
            m.editPatient("P1", PatientCategory::General,
                          detail("P1", "Ada " + QString::number(i)));
            persistence::RecoveryService::saveWithBackup(m.database(), file);
            QTest::qWait(2);
        }
        QCOMPARE(persistence::RecoveryService::copies(file).size(), 5);
        m.editPatient("P1", PatientCategory::General, detail("P1", "Recovered Ada"));
        persistence::RecoveryService::autosave(m.database(), file);
        auto recovery = FileHandler::load(persistence::RecoveryService::recoveryPath(file));
        QCOMPARE(recovery.patient("P1")->details().name, QString("Recovered Ada"));
        bytes[bytes.size() / 2] = static_cast<char>(bytes[bytes.size() / 2] ^ 1);
        QVERIFY(contents.open(QIODevice::WriteOnly));
        contents.write(bytes);
        contents.close();
        QVERIFY_EXCEPTION_THROWN(FileHandler::load(file), FileError);
        persistence::RecoveryService::discardRecovery(file);
        QVERIFY(!QFileInfo::exists(persistence::RecoveryService::recoveryPath(file)));
#endif
    }
    void dateRangeReportTotalsAndEscaping() {
        auto db = fixture();
        db.appointments[0].start = QDateTime(QDate(2026, 1, 1), QTime(9, 0));
        db.appointments[1].start = QDateTime(QDate(2026, 1, 2), QTime(9, 0));
        auto report = generateReport(db, ReportType::Daily, QDate(2026, 1, 1), QDate(2026, 1, 2));
        QCOMPARE(report.rows[0][1], QString("2"));
        QVERIFY(report.summary.contains("2 visits"));
        QVERIFY_EXCEPTION_THROWN(
            generateReport(db, ReportType::Daily, QDate(2026, 1, 3), QDate(2026, 1, 1)),
            ValidationError);
        Report html{"<script>", {"A&B"}, {{"<b>literal</b>"}}, "<summary>"};
        const auto output = reportHtml(html);
        QVERIFY(!output.contains("<script>"));
        QVERIFY(output.contains("&lt;b&gt;literal"));
    }
    void guiRetainsSearchSortAndSelectionAfterEdit() {
        gui::MainWindow window;
        window.manager().replace(fixture());
        window.refresh();
        window.show();
        auto *nav = window.findChild<QListWidget *>("navigation");
        nav->setCurrentRow(4);
        window.findChild<QComboBox *>("searchField")->setCurrentIndex(1);
        window.findChild<QLineEdit *>("searchText")->setText("Ada");
        QTest::mouseClick(window.findChild<QPushButton *>("searchButton"), Qt::LeftButton);
        QCOMPARE(window.findChild<QTableView *>("searchResults")->model()->rowCount(), 1);
        nav->setCurrentRow(1);
        auto *table = window.findChild<QTableView *>("patientsTable");
        table->selectRow(0);
        QTimer::singleShot(30, [&] {
            auto *dialog = window.findChild<QDialog *>("patientDialog");
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("patientName")->setText("Ada Updated");
            dialog->findChild<QDialogButtonBox *>("patientButtons")
                ->button(QDialogButtonBox::Save)
                ->click();
        });
        window.findChild<QPushButton *>("editPatient")->click();
        QCOMPARE(table->currentIndex().row(), 0);
        auto *results = window.findChild<QTableView *>("searchResults")->model();
        QCOMPARE(results->rowCount(), 1);
        QCOMPARE(results->index(0, 1).data().toString(), QString("Ada Updated"));
        nav->setCurrentRow(4);
        window.findChild<QComboBox *>("sortField")->setCurrentIndex(2);
        window.findChild<QPushButton *>("sortButton")->click();
        window.findChild<QPushButton *>("undoButton")->click();
        QCOMPARE(results->rowCount(), 3);
        QCOMPARE(results->index(0, 2).data().toInt(), 23);
        window.manager().markSaved();
        window.close();
    }
    void guiUnsavedPromptAndRescheduling() {
        gui::MainWindow window;
        window.manager().replace(fixture());
        window.refresh();
        window.show();
        window.findChild<QListWidget *>("navigation")->setCurrentRow(2);
        auto *table = window.findChild<QTableWidget *>("appointmentsTable");
        table->selectRow(0);
        QTimer::singleShot(30, [&] {
            auto *dialog = window.findChild<QDialog *>("rescheduleDialog");
            QVERIFY(dialog);
            dialog->findChild<QDateTimeEdit *>("rescheduleTime")
                ->setDateTime(QDateTime::currentDateTime().addDays(2));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        window.findChild<QPushButton *>("rescheduleAppointment")->click();
        QVERIFY(window.manager().database().appointment("A1")->start >
                QDateTime::currentDateTime());
        QTimer::singleShot(30, [&] {
            auto *box = window.findChild<QMessageBox *>();
            QVERIFY(box);
            box->button(QMessageBox::Cancel)->click();
        });
        window.findChild<QAction *>("newWorkspaceAction")->trigger();
        QCOMPARE(window.manager().database().patients.size(), size_t(3));
        QVERIFY(window.manager().dirty());
        QTimer::singleShot(30, [&] {
            auto *box = window.findChild<QMessageBox *>();
            QVERIFY(box);
            box->button(QMessageBox::Discard)->click();
        });
        window.findChild<QAction *>("newWorkspaceAction")->trigger();
        QVERIFY(window.manager().database().patients.empty());
        window.close();
    }

    void walkInNoShowAndCorrections() {
        ClinicManager m;
        m.replace(fixture());
        m.enqueue("P3");
        const auto visit = m.database().waiting.front().appointmentId;
        QVERIFY(!visit.isEmpty());
        QVERIFY(m.database().appointment(visit)->walkIn);
        QVERIFY_EXCEPTION_THROWN(m.complete(visit, "Too soon"), ValidationError);
        m.callNext();
        m.complete(visit, "Initial notes");
        QVERIFY(m.database().appointment(visit)->completedAt.isValid());
        m.correctNotes(visit, "Corrected notes", "Clarified wording");
        QCOMPARE(m.database().appointment(visit)->corrections.size(), 1);
        QVERIFY(m.database().appointment(visit)->corrections[0].contains("Initial notes"));
        m.noShow("A1", "Patient did not attend");
        QCOMPARE(m.database().appointment("A1")->status, AppointmentStatus::NoShow);
        QVERIFY_EXCEPTION_THROWN(m.noShow("A3", "Already cancelled"), ValidationError);
        const auto future = m.schedule("P1", "Future", QDateTime::currentDateTime().addDays(1), 30);
        QVERIFY_EXCEPTION_THROWN(m.noShow(future, "Early"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.cancel(future), ValidationError);
        const auto report = generateReport(m.database(), ReportType::Daily);
        QCOMPARE(report.rows.last(), QStringList({"No-show", "1"}));
        const auto restored = FileHandler::deserialize(FileHandler::serialize(m.database()));
        QCOMPARE(restored.appointment(visit)->corrections,
                 m.database().appointment(visit)->corrections);
        QVERIFY(restored.appointment(visit)->walkIn);
    }
    void clinicianRoomConflictsAndPersistence() {
        ClinicManager m;
        m.replace(fixture());
        m.addResource("Dr Ada", false);
        m.addResource("Dr Tunde", false);
        m.addResource("Room 1", true);
        m.addResource("Room 2", true);
        QVERIFY_EXCEPTION_THROWN(m.addResource("room 1", true), ValidationError);
        auto start = QDateTime::currentDateTime().addDays(1);
        const auto a = m.schedule("P1", "Review", start, 30, "Dr Ada", "Room 1");
        auto before = stateOnly(m.database());
        QVERIFY_EXCEPTION_THROWN(m.schedule("P2", "Review", start, 30, "Dr Ada", "Room 2"),
                                 ValidationError);
        QVERIFY_EXCEPTION_THROWN(m.schedule("P2", "Review", start, 30, "Dr Tunde", "Room 1"),
                                 ValidationError);
        QCOMPARE(stateOnly(m.database()), before);
        const auto b = m.schedule("P2", "Review", start, 30, "Dr Tunde", "Room 2");
        QVERIFY_EXCEPTION_THROWN(m.assignResources(b, "Dr Ada", "Room 2"), ValidationError);
        m.reschedule(b, start.addSecs(1800), 30);
        m.assignResources(b, "Dr Ada", "Room 1");
        QCOMPARE(m.database().appointment(a)->room, QString("Room 1"));
        const auto restored = FileHandler::deserialize(FileHandler::serialize(m.database()));
        QCOMPARE(restored.clinicians, m.database().clinicians);
        QCOMPARE(restored.appointment(b)->clinician, QString("Dr Ada"));
    }
    void workspaceLockAndExternalChange() {
        QTemporaryDir dir;
        const auto file = dir.filePath("workspace.json");
        auto db = fixture();
        FileHandler::save(db, file);
        {
            persistence::WorkspaceGuard guard(file);
            guard.verifyUnchanged();
            QVERIFY_EXCEPTION_THROWN(persistence::WorkspaceGuard second(file), FileError);
            db.nextPatientId += 10;
            FileHandler::save(db, file);
            QVERIFY_EXCEPTION_THROWN(guard.verifyUnchanged(), FileError);
            guard.checkpoint();
            guard.verifyUnchanged();
        }
        persistence::WorkspaceGuard reopened(file);
        reopened.verifyUnchanged();
        gui::MainWindow window;
        QVERIFY_EXCEPTION_THROWN(window.loadPath(file), FileError);
    }
    void portableBackupAuthentication() {
#ifdef Q_OS_WIN
        // Independent fixture generated with Python cryptography AESGCM and hashlib PBKDF2.
        const auto external =
            QByteArray::fromBase64("U0NBUE1TLUJBQ0tVUC0xCgABAgMEBQYHCAkKCwwNDg8AAQIDBAUGBwgJCgvhpxT"
                                   "wL9As2ZdaFaEjkqKTcWsk8lmPFd/kqzk3u8w+C7lZC3vHQbagOxWEsr4ABI4n");
        QCOMPARE(security::decryptBackup(external, "interoperability test password"),
                 QByteArray("portable backup interoperability\n"));
        const auto plain = QJsonDocument(FileHandler::serialize(fixture())).toJson();
        const auto password = QString("a separate long backup passphrase");
        const auto encrypted = security::encryptBackup(plain, password);
        QVERIFY(encrypted.startsWith("SCAPMS-BACKUP-1\n"));
        QVERIFY(!encrypted.contains("Health review"));
        QCOMPARE(security::decryptBackup(encrypted, password), plain);
        QVERIFY_EXCEPTION_THROWN(security::decryptBackup(encrypted, "wrong but sufficiently long"),
                                 FileError);
        auto tampered = encrypted;
        tampered[tampered.size() - 1] ^= 1;
        QVERIFY_EXCEPTION_THROWN(security::decryptBackup(tampered, password), FileError);
        QVERIFY_EXCEPTION_THROWN(security::encryptBackup(plain, "short"), ValidationError);
        QVERIFY_EXCEPTION_THROWN(security::decryptBackup(encrypted.left(20), password), FileError);
#endif
    }
    void clinicalPrivacyAndExportPermissions() {
        ClinicManager m;
        m.replace(fixture());
        m.configureAdministrator("admin", "long administrator password");
        m.addUser("desk", "long receptionist password", security::Role::Receptionist);
        m.addUser("reader", "long viewer password", security::Role::Viewer);
        m.lock();
        m.login("desk", "long receptionist password");
        const auto old = m.database().patient("P1")->details().reason;
        auto change = detail("P1", "Updated demographics");
        change.reason = "Unauthorized clinical change";
        m.editPatient("P1", PatientCategory::Emergency, change);
        QCOMPARE(m.database().patient("P1")->details().reason, old);
        QCOMPARE(m.database().patient("P1")->category(), PatientCategory::General);
        QVERIFY_EXCEPTION_THROWN(m.requireReportExport(), ValidationError);
        gui::PatientTableModel model;
        model.setPatients(viewOf(m.database().patients));
        QVERIFY(!model.data(model.index(0, 0), Qt::ToolTipRole).toString().contains(old));
        model.setClinicalAccess(true);
        QVERIFY(model.data(model.index(0, 0), Qt::ToolTipRole).toString().contains(old));
        gui::PatientDialog dialog("P1", m.database().patient("P1"));
        dialog.configureAccess(false, m.database());
        QVERIFY(dialog.findChild<QLineEdit *>("patientReason")->isHidden());
        QVERIFY(dialog.findChild<QComboBox *>("patientCategory")->isHidden());
        gui::MainWindow window;
        window.manager().replace(m.database(), "reader", "long viewer password");
        window.refresh();
        QVERIFY(!window.findChild<QPushButton *>("exportReport")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("exportReportPdf")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("printReport")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("addClinician")->isEnabled());
    }
    void contactDuplicatesAndPreviousVisitPicker() {
        auto invalid = detail("P9");
        invalid.contact = "not a contact";
        QVERIFY_EXCEPTION_THROWN(makePatient(PatientCategory::General, invalid), ValidationError);
        invalid.contact = "+234 801 234 5678";
        QVERIFY(makePatient(PatientCategory::General, invalid));
        ClinicManager m;
        m.replace(fixture());
        auto duplicate = detail("P8", "Ada", 23);
        QVERIFY(!m.possibleDuplicates(duplicate).isEmpty());
        QVERIFY(m.possibleDuplicates(detail("P1", "Ada", 23), "P1").isEmpty());
        gui::PatientDialog dialog("P2", m.database().patient("P2"));
        dialog.configureAccess(true, m.database());
        auto *choices = dialog.findChild<QComboBox *>("previousVisitChoices");
        QVERIFY(choices);
        QVERIFY(choices->findData("A2") >= 0);
        QVERIFY(dialog.findChild<QLineEdit *>("previousVisit")->isReadOnly());
    }
    void repeatedBenchmarksUseIdenticalCases() {
        ClinicDatabase db;
        for (int i = 0; i < 10; ++i)
            db.patients.push_back(
                makePatient(PatientCategory::General, detail("P" + QString::number(i))));
        const auto results = benchmarkSorts(viewOf(db.patients), SortField::Id, 3);
        QCOMPARE(results.size(), size_t(6));
        QCOMPARE(results[0].metrics.comparisons, size_t(9));
        QCOMPARE(results[2].metrics.comparisons, size_t(45));
        for (const auto &r : results) {
            QCOMPARE(r.records, 10);
            QVERIFY(r.medianMs >= 0);
        }
        QVERIFY_EXCEPTION_THROWN(benchmarkSorts({}, SortField::Id), ValidationError);
    }
    void welcomeAndCalendarUi() {
        gui::MainWindow window;
        QTimer::singleShot(30, [&] {
            auto *button = window.findChild<QPushButton *>("welcomeCreate");
            QVERIFY(button);
            button->click();
        });
        QVERIFY(window.welcome());
        QVERIFY(window.manager().database().patients.empty());
        window.manager().replace(fixture());
        window.manager().addResource("Dr Calendar", false);
        window.manager().addResource("Room Calendar", true);
        window.refresh();
        auto *nav = window.findChild<QListWidget *>("navigation");
        QCOMPARE(nav->count(), 8);
        nav->setCurrentRow(7);
        auto *calendar = window.findChild<QCalendarWidget *>("resourceCalendar");
        QVERIFY(calendar);
        calendar->setSelectedDate(QDate::currentDate());
        QVERIFY(window.findChild<QComboBox *>("calendarClinician")->findData("Dr Calendar") >= 0);
        window.manager().markSaved();
    }

    void guiDynamicFieldsAndValidation() {
        gui::PatientDialog dialog("P1");
        dialog.show();
        auto *category = dialog.findChild<QComboBox *>("patientCategory");
        category->setCurrentIndex(1);
        QVERIFY(!dialog.findChild<QSpinBox *>("patientUrgency")->isHidden());
        QVERIFY(dialog.findChild<QLineEdit *>("previousVisit")->isHidden());
        category->setCurrentIndex(2);
        QVERIFY(!dialog.findChild<QLineEdit *>("previousVisit")->isHidden());
        QVERIFY(dialog.findChild<QSpinBox *>("patientUrgency")->isHidden());
        QTest::mouseClick(
            dialog.findChild<QDialogButtonBox *>("patientButtons")->button(QDialogButtonBox::Save),
            Qt::LeftButton);
        QCOMPARE(dialog.result(), 0);
        QVERIFY(!dialog.findChild<QLabel *>("formError")->text().isEmpty());
    }
};
QTEST_MAIN(ClinicTests)
#include "ClinicTests.moc"

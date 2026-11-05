#include "MainWindow.h"
#include "PatientDialog.h"
#include "security/PortableBackup.h"
#include "panels/CalendarPanel.h"
#include <QSettings>
#include <QJsonDocument>
#include <QSaveFile>
#include "Theme.h"
#include "model/Errors.h"
#include "panels/AccessPanel.h"
#include "panels/AppointmentsPanel.h"
#include "panels/DashboardPanel.h"
#include "panels/PatientsPanel.h"
#include "panels/QueuePanel.h"
#include "panels/ReportsPanel.h"
#include "panels/SearchPanel.h"
#include "persistence/RecoveryService.h"
#include <QtWidgets>
namespace clinic::gui {
using persistence::FileHandler;
using persistence::RecoveryService;
namespace {
bool credentials(QWidget *parent, const QString &title, QString &username, QString &password,
                 bool create = false) {
    QDialog dialog(parent);
    dialog.setObjectName("credentialsDialog");
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(430);
    auto *form = new QFormLayout(&dialog);
    auto *info =
        new QLabel(create ? "Choose your administrator credentials. There is no default password."
                          : "Sign in with a staff account for this workspace.");
    info->setWordWrap(true);
    form->addRow(info);
    auto *name = new QLineEdit;
    name->setObjectName("loginUsername");
    name->setText(username);
    auto *secret = new QLineEdit;
    secret->setObjectName("loginPassword");
    secret->setEchoMode(QLineEdit::Password);
    form->addRow("Username", name);
    form->addRow(create ? "Password (12+ characters)" : "Password", secret);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    username = name->text();
    password = secret->text();
    return true;
}
} // namespace
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setObjectName("mainWindow");
    setMinimumSize(1180, 740);
    resize(1380, 880);
    setStyleSheet(theme);
    auto *shell = new QWidget;
    auto *horizontal = new QHBoxLayout(shell);
    horizontal->setContentsMargins(0, 0, 0, 0);
    horizontal->setSpacing(0);
    auto *sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(218);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(18, 24, 18, 20);
    auto *brand = new QLabel("MIVA / CLINIC");
    brand->setObjectName("brand");
    side->addWidget(brand);
    auto *caption = new QLabel("PATIENT CARE DESK");
    caption->setObjectName("brandCaption");
    side->addWidget(caption);
    navigation_ = new QListWidget;
    navigation_->setObjectName("navigation");
    navigation_->addItems({"Overview", "Patient records", "Appointments", "Waiting queue",
                           "Search & sort", "Reports", "Staff & activity", "Calendar & rooms"});
    side->addWidget(navigation_);
    auto *foot = new QLabel("SCAPMS 3.0 / COS 202");
    foot->setObjectName("brandCaption");
    side->addWidget(foot);
    horizontal->addWidget(sidebar);
    pages_ = new QStackedWidget;
    pages_->setObjectName("pages");
    MutationHandler mutation = [this](const std::function<void()> &action, const QString &success) {
        run(action, success);
    };
    panels_ = {new DashboardPanel(manager_, mutation),
               new PatientsPanel(manager_, mutation),
               new AppointmentsPanel(manager_, mutation),
               new QueuePanel(manager_, mutation),
               new SearchPanel(manager_, mutation),
               new ReportsPanel(manager_, mutation),
               new AccessPanel(manager_, mutation, [this] { protectWorkspace(); }),
               new CalendarPanel(manager_, mutation)};
    for (auto *panel : panels_)
        pages_->addWidget(panel);
    horizontal->addWidget(pages_, 1);
    setCentralWidget(shell);
    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    navigation_->setCurrentRow(0);
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int i) {
        if (i >= 0 && i < static_cast<int>(panels_.size()))
            panels_[i]->refresh();
    });
    auto *file = menuBar()->addMenu("&File");
    auto *fresh = file->addAction("&New workspace");
    fresh->setObjectName("newWorkspaceAction");
    fresh->setShortcut(QKeySequence::New);
    connect(fresh, &QAction::triggered, this, [this] {
        if (confirmDiscard()) {
            prepareChange();
            manager_.newDatabase();
            currentPath_.clear();
            workspaceGuard_.reset();
            refresh();
        }
    });
    auto *load = file->addAction("&Load workspace...");
    load->setShortcut(QKeySequence::Open);
    connect(load, &QAction::triggered, this, &MainWindow::openFile);
    auto *saveAction = file->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] { save(); });
    auto *saveAs = file->addAction("Save &as...");
    saveAs->setShortcut(QKeySequence::SaveAs);
    connect(saveAs, &QAction::triggered, this, [this] { save(true); });
    auto *exportBackup = file->addAction("Export portable encrypted backup...");
    exportBackup->setObjectName("exportPortableBackup");
    connect(exportBackup, &QAction::triggered, this, [this] { portableBackup(false); });
    auto *importBackup = file->addAction("Import portable encrypted backup...");
    connect(importBackup, &QAction::triggered, this, [this] { portableBackup(true); });
    auto *home = file->addAction("Welcome / recent workspaces...");
    connect(home, &QAction::triggered, this, [this] {
        if (confirmDiscard())
            welcome();
    });
    auto *view = menuBar()->addMenu("&View");
    for (int size : {10, 12, 14}) {
        auto *action = view->addAction(QString("Text size %1 pt").arg(size));
        connect(action, &QAction::triggered, this, [this, size] { setTextSize(size); });
    }
    for (int i = 0; i < 8; ++i) {
        auto *shortcut = new QShortcut(QKeySequence(QString("Alt+%1").arg(i + 1)), this);
        connect(shortcut, &QShortcut::activated, this,
                [this, i] { navigation_->setCurrentRow(i); });
    }
    auto *recovery = file->addAction("Recover autosave / backup...");
    recovery->setObjectName("recoverAction");
    connect(recovery, &QAction::triggered, this, &MainWindow::recover);
    file->addSeparator();
    auto *quit = file->addAction("E&xit");
    quit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("&Undo");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        run(
            [this] {
                manager_.undo();
                dynamic_cast<QueuePanel *>(panels_[3])->resetMessage();
            },
            "Operation undone.");
    });
    auto *find = edit->addAction("&Find patients");
    find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, [this] {
        navigation_->setCurrentRow(4);
        findChild<QLineEdit *>("searchText")->setFocus();
    });
    toolbar_ = addToolBar("Workspace");
    toolbar_->setMovable(false);
    auto *open = actionButton("Load data", "loadButton");
    toolbar_->addWidget(open);
    connect(open, &QPushButton::clicked, this, &MainWindow::openFile);
    auto *saveButton = actionButton("Save workspace", "saveButton", true);
    saveButton->setToolTip("Save with a protected backup of the previous version (Ctrl+S)");
    toolbar_->addWidget(saveButton);
    connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    undoButton_ = actionButton("Undo", "undoButton");
    toolbar_->addWidget(undoButton_);
    connect(undoButton_, &QPushButton::clicked, undoAction_, &QAction::trigger);
    auto *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);
    fileLabel_ = new QLabel;
    toolbar_->addWidget(fileLabel_);
    sessionLabel_ = new QLabel;
    statusBar()->addPermanentWidget(sessionLabel_);
    lockButton_ = actionButton("Lock / switch user", "lockButton");
    toolbar_->addWidget(lockButton_);
    connect(lockButton_, &QPushButton::clicked, this, &MainWindow::lockWorkspace);
    auto *timer = new QTimer(this);
    timer->setInterval(30000);
    connect(timer, &QTimer::timeout, this, [this] {
        if (locking_)
            return;
        if (!manager_.demoMode() && manager_.authenticated() && idle_.elapsed() >= 300000) {
            lockWorkspace();
            return;
        }
        panels_[0]->refresh();
        panels_[3]->refresh();
        autosaveNow();
    });
    timer->start();
    qApp->installEventFilter(this);
    idle_.start();
    refresh();
}
MainWindow::~MainWindow() {
    qApp->removeEventFilter(this);
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress ||
        event->type() == QEvent::Wheel)
        idle_.restart();
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::prepareChange() {
    for (auto *panel : panels_)
        panel->beforeChange();
}
void MainWindow::run(const std::function<void()> &operation, const QString &success) {
    prepareChange();
    try {
        operation();
        refresh();
        statusBar()->showMessage(success, 6000);
    } catch (const std::exception &e) {
        refresh();
        QMessageBox::warning(this, "Unable to complete operation", QString::fromUtf8(e.what()));
    }
}
void MainWindow::refresh() {
    for (auto *panel : panels_)
        panel->refresh();
    updateTitle();
}
void MainWindow::updateTitle() {
    const auto name =
        currentPath_.isEmpty() ? "Untitled workspace" : QFileInfo(currentPath_).fileName();
    setWindowTitle(QString("SCAPMS 3.0 - %1%2").arg(name, manager_.dirty() ? " *" : ""));
    fileLabel_->setText(name + (manager_.dirty() ? " / Unsaved" : " / Saved"));
    sessionLabel_->setText(manager_.demoMode()
                               ? "DEMO / fictional data only"
                               : manager_.username() + " / " + security::roleName(manager_.role()));
    undoAction_->setEnabled(manager_.canUndo());
    undoButton_->setEnabled(manager_.canUndo());
    undoAction_->setText(manager_.canUndo() ? "Undo: " + manager_.undoDescription() : "Undo");
    undoButton_->setText("Undo");
    undoButton_->setToolTip(manager_.canUndo() ? "Undo: " + manager_.undoDescription()
                                               : "No operation to undo");
    lockButton_->setEnabled(!manager_.demoMode());
    findChild<QPushButton *>("saveButton")->setEnabled(manager_.canEdit());
    findChild<QAction *>("exportPortableBackup")->setEnabled(manager_.isAdmin());
}
bool MainWindow::save(bool choosePath) {
    if (!manager_.canEdit()) {
        QMessageBox::warning(this, "Save", "Your role cannot save or copy a workspace.");
        return false;
    }
    auto target = currentPath_;
    if (!manager_.demoMode() && QFileInfo(target).suffix() == "json")
        choosePath = true;
    if (target.isEmpty() || choosePath) {
        const auto filter = manager_.demoMode()
                                ? "Protected workspace (*.clinic);;Demo JSON (*.json)"
                                : "Protected workspace (*.clinic)";
        target = QFileDialog::getSaveFileName(
            this, "Save workspace",
            manager_.demoMode() ? "clinic-data.clinic" : "clinic-workspace.clinic", filter);
    }
    if (target.isEmpty())
        return false;
    if (QFileInfo(target).suffix().isEmpty())
        target += ".clinic";
    try {
        std::unique_ptr<persistence::WorkspaceGuard> nextGuard;
        auto *guard = workspaceGuard_.get();
        const auto resolved = QFileInfo(target).exists() ? QFileInfo(target).canonicalFilePath()
                                                         : QFileInfo(target).absoluteFilePath();
        if (!guard || guard->path() != resolved) {
            nextGuard = std::make_unique<persistence::WorkspaceGuard>(target);
            guard = nextGuard.get();
        }
        guard->verifyUnchanged();
        const auto oldRecovery = RecoveryService::recoveryPath(currentPath_);
        RecoveryService::saveWithBackup(manager_.database(), target);
        if (oldRecovery != RecoveryService::recoveryPath(target))
            QFile::remove(oldRecovery);
        guard->checkpoint();
        currentPath_ = guard->path();
        if (nextGuard)
            workspaceGuard_ = std::move(nextGuard);
        rememberWorkspace();
        manager_.markSaved();
        updateTitle();
        statusBar()->showMessage(
            "Saved. Up to five previous versions are kept as protected backups.", 6000);
        return true;
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Save failed", e.what());
        return false;
    }
}
bool MainWindow::confirmDiscard() {
    if (!manager_.dirty())
        return true;
    if (!manager_.canEdit()) {
        autosaveNow();
        return true;
    }
    const auto choice = QMessageBox::question(
        this, "Unsaved changes", "Save changes to the current workspace?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Save)
        return save();
    if (choice == QMessageBox::Discard) {
        try {
            RecoveryService::discardRecovery(currentPath_);
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Recovery copy", e.what());
            return false;
        }
        return true;
    }
    return false;
}
void MainWindow::loadPath(const QString &path, const QString &username, const QString &password) {
    std::unique_ptr<persistence::WorkspaceGuard> nextGuard;
    auto *guard = workspaceGuard_.get();
    if (!guard || guard->path() != QFileInfo(path).canonicalFilePath()) {
        nextGuard = std::make_unique<persistence::WorkspaceGuard>(path);
        guard = nextGuard.get();
    } else {
        // A reload intentionally accepts the current on-disk version.
        guard->checkpoint();
    }
    controller::ClinicManager next;
    next.replace(FileHandler::load(path), username, password);
    guard->verifyUnchanged();
    prepareChange();
    manager_ = std::move(next);
    if (nextGuard)
        workspaceGuard_ = std::move(nextGuard);
    currentPath_ = guard->path();
    rememberWorkspace();
    dynamic_cast<QueuePanel *>(panels_[3])->resetMessage();
    refresh();
    idle_.restart();
}
bool MainWindow::openWorkspace(const QString &path) {
    try {
        const auto candidate = FileHandler::load(path);
        if (candidate.users.empty()) {
            loadPath(path);
            return true;
        }
        QString name, password;
        for (int attempt = 0; attempt < 5; ++attempt) {
            if (!credentials(this, "Open protected workspace", name, password))
                return false;
            try {
                loadPath(path, name, password);
                return true;
            } catch (const model::ValidationError &e) {
                QMessageBox::warning(this, "Sign in", e.what());
            }
        }
        QMessageBox::warning(this, "Sign in",
                             "Too many attempts. Opening this workspace has been cancelled.");
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Load failed", e.what());
    }
    return false;
}
void MainWindow::openFile() {
    if (!confirmDiscard())
        return;
    const auto file =
        QFileDialog::getOpenFileName(this, "Load workspace", {}, "Clinic data (*.clinic *.json)");
    if (file.isEmpty())
        return;
    if (openWorkspace(file))
        offerRecovery();
}
void MainWindow::autosaveNow() {
    if (!manager_.dirty())
        return;
    try {
        if (workspaceGuard_)
            workspaceGuard_->verifyUnchanged();
        RecoveryService::autosave(manager_.database(), currentPath_);
        recoveryError_.clear();
        statusBar()->showMessage("Recovery copy updated; use Save to commit the workspace.", 4000);
    } catch (const std::exception &e) {
        recoveryError_ = QString::fromUtf8(e.what());
        statusBar()->showMessage("Recovery unavailable: " + recoveryError_);
    }
}
void MainWindow::protectWorkspace() {
    QString username, password;
    if (!credentials(this, "Protect workspace", username, password, true))
        return;
    auto file =
        QFileDialog::getSaveFileName(this, "Save protected workspace", "clinic-workspace.clinic",
                                     "Protected workspace (*.clinic)");
    if (file.isEmpty())
        return;
    if (!file.endsWith(".clinic", Qt::CaseInsensitive))
        file += ".clinic";
    run(
        [&] {
            controller::ClinicManager candidate;
            candidate.replace(manager_.database());
            candidate.configureAdministrator(username, password);
            auto guard = std::make_unique<persistence::WorkspaceGuard>(file);
            guard->verifyUnchanged();
            RecoveryService::saveWithBackup(candidate.database(), file);
            guard->checkpoint();
            manager_ = std::move(candidate);
            manager_.markSaved();
            currentPath_ = guard->path();
            workspaceGuard_ = std::move(guard);
            rememberWorkspace();
        },
        "Access control enabled. Keep backups with this Windows account.");
}
void MainWindow::lockWorkspace() {
    if (manager_.demoMode() || locking_)
        return;
    locking_ = true;
    manager_.lock();
    autosaveNow();
    centralWidget()->hide();
    toolbar_->hide();
    menuBar()->hide();
    sessionLabel_->setText("Workspace locked");
    QString username, password;
    bool unlocked = false;
    while (credentials(this, "Unlock workspace", username, password)) {
        try {
            manager_.login(username, password);
            unlocked = true;
            break;
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Sign in", e.what());
        }
    }
    if (!unlocked) {
        autosaveNow();
        qApp->quit();
        return;
    }
    centralWidget()->show();
    toolbar_->show();
    menuBar()->show();
    idle_.restart();
    locking_ = false;
    refresh();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (confirmDiscard())
        event->accept();
    else
        event->ignore();
}
void MainWindow::captureScreenshots(const QString &directory) {
    QDir().mkpath(directory);
    for (const auto &p : manager_.database().patients)
        manager_.access(p->id());
    refresh();
    const QStringList names{"01-dashboard",       "02-patients",      "04-appointments",
                            "05-waiting-queue",   "06-search-sort",   "07-reports",
                            "08-access-activity", "11-calendar-rooms"};
    for (int i = 0; i < 8; ++i) {
        navigation_->setCurrentRow(i);
        if (i == 1 && findChild<QTableView *>("patientsTable")->model()->rowCount())
            findChild<QTableView *>("patientsTable")->selectRow(0);
        if (i == 4) {
            findChild<QComboBox *>("sortAlgorithm")->setCurrentIndex(1);
            findChild<QPushButton *>("sortButton")->click();
        }
        if (i == 5)
            findChild<QComboBox *>("reportType")->setCurrentIndex(3);
        QApplication::processEvents();
        if (!grab().save(QDir(directory).filePath(names[i] + ".png")))
            throw model::FileError("Could not save screenshot.");
    }
    PatientDialog registration(manager_.suggestedPatientId(), nullptr, this);
    registration.findChild<QLineEdit *>("patientName")->setText("Ada Okafor");
    registration.findChild<QLineEdit *>("patientContact")->setText("ada@example.test");
    registration.findChild<QLineEdit *>("patientReason")->setText("Routine health review");
    registration.findChild<QSpinBox *>("patientAge")->setValue(23);
    registration.show();
    QApplication::processEvents();
    registration.grab().save(QDir(directory).filePath("03-registration.png"));
    registration.close();
    navigation_->setCurrentRow(1);
    QTimer::singleShot(100, this, [this, directory] {
        if (auto *dialog = findChild<QDialog *>("historyDialog")) {
            dialog->grab().save(QDir(directory).filePath("09-patient-history.png"));
            dialog->reject();
        }
    });
    findChild<QPushButton *>("patientHistory")->click();
    navigation_->setCurrentRow(4);
    auto *captureTimer = new QTimer(this);
    connect(captureTimer, &QTimer::timeout, this, [this, directory, captureTimer] {
        if (auto *dialog = findChild<QDialog *>("benchmarkDialog")) {
            dialog->grab().save(QDir(directory).filePath("10-algorithm-comparison.png"));
            captureTimer->stop();
            captureTimer->deleteLater();
            dialog->reject();
        }
    });
    captureTimer->start(100);
    findChild<QPushButton *>("benchmarkButton")->click();
    navigation_->setCurrentRow(0);
    QTimer::singleShot(100, this, [this, directory] {
        if (auto *dialog = findChild<QDialog *>("welcomeDialog")) {
            dialog->grab().save(QDir(directory).filePath("12-welcome.png"));
            dialog->reject();
        }
    });
    welcome();
}
} // namespace clinic::gui

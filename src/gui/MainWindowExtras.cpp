#include "MainWindow.h"
#include "Theme.h"
#include "model/Errors.h"
#include "persistence/FileHandler.h"
#include "persistence/RecoveryService.h"
#include "security/PortableBackup.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QSaveFile>
namespace clinic::gui {
using persistence::FileHandler;
using persistence::RecoveryService;
void MainWindow::rememberWorkspace() {
    if (currentPath_.isEmpty() || currentPath_.endsWith(".recovery.clinic") ||
        currentPath_.contains(".backups/") || currentPath_.contains(".backups\\"))
        return;
    QSettings settings;
    auto recent = settings.value("recentWorkspaces").toStringList();
    recent.removeAll(currentPath_);
    recent.prepend(currentPath_);
    while (recent.size() > 10)
        recent.removeLast();
    settings.setValue("recentWorkspaces", recent);
}
void MainWindow::setTextSize(int size) {
    size = qBound(10, size, 14);
    QString css = QString::fromUtf8(theme);
    css.replace("font-size:10pt", QString("font-size:%1pt").arg(size));
    setStyleSheet(css);
    QSettings().setValue("textSize", size);
}
bool MainWindow::welcome() {
    QDialog dialog(this);
    dialog.setWindowTitle("Welcome to SCAPMS 3.0");
    dialog.setObjectName("welcomeDialog");
    dialog.resize(650, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel("Open your clinic workspace");
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto *hint =
        new QLabel("Choose a saved workspace, start a new one, or explore fictional demo records.");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *recent = new QListWidget;
    recent->setObjectName("recentWorkspaces");
    for (const auto &p : QSettings().value("recentWorkspaces").toStringList()) {
        if (!QFileInfo::exists(p))
            continue;
        auto *item = new QListWidgetItem(QFileInfo(p).fileName(), recent);
        item->setData(Qt::UserRole, p);
        item->setToolTip(p);
    }
    layout->addWidget(new QLabel(recent->count() ? "Recent workspaces (double-click to open)"
                                                 : "No recent workspaces yet."));
    layout->addWidget(recent);
    auto choose = [&](const QString &p) {
        if (!p.isEmpty() && openWorkspace(p)) {
            offerRecovery();
            dialog.accept();
        }
    };
    connect(recent, &QListWidget::itemDoubleClicked, &dialog,
            [&](QListWidgetItem *item) { choose(item->data(Qt::UserRole).toString()); });
    auto *row = new QHBoxLayout;
    auto *open = new QPushButton("Open workspace");
    auto *fresh = new QPushButton("Create new");
    auto *demo = new QPushButton("Try demo");
    open->setObjectName("welcomeOpen");
    fresh->setObjectName("welcomeCreate");
    demo->setObjectName("welcomeDemo");
    row->addWidget(open);
    row->addWidget(fresh);
    row->addWidget(demo);
    layout->addLayout(row);
    connect(open, &QPushButton::clicked, &dialog, [&] {
        choose(QFileDialog::getOpenFileName(&dialog, "Open workspace", {},
                                            "Clinic workspace (*.clinic *.json)"));
    });
    connect(fresh, &QPushButton::clicked, &dialog, [&] {
        prepareChange();
        manager_.newDatabase();
        currentPath_.clear();
        workspaceGuard_.reset();
        refresh();
        dialog.accept();
    });
    connect(demo, &QPushButton::clicked, &dialog, [&] {
        try {
            auto sample = QCoreApplication::applicationDirPath() + "/sample-clinic.json";
            if (!QFileInfo::exists(sample))
                sample = QCoreApplication::applicationDirPath() + "/../data/sample-clinic.json";
            auto db = FileHandler::load(sample);
            prepareChange();
            manager_.replace(std::move(db));
            currentPath_.clear();
            workspaceGuard_.reset();
            refresh();
            dialog.accept();
        } catch (const std::exception &e) {
            QMessageBox::warning(&dialog, "Demo unavailable", e.what());
        }
    });
    auto *recoverButton = new QPushButton("Review local recovery copies");
    auto *importButton = new QPushButton("Import portable encrypted backup");
    try {
        const auto count = RecoveryService::copies({}).size();
        if (count)
            layout->addWidget(new QLabel(
                QString("%1 local recovery copies are available for review.").arg(count)));
    } catch (const std::exception &e) {
        auto *message = new QLabel(QString("Recovery folder unavailable: %1").arg(e.what()));
        message->setWordWrap(true);
        layout->addWidget(message);
    }
    layout->addWidget(recoverButton);
    layout->addWidget(importButton);
    connect(recoverButton, &QPushButton::clicked, &dialog, [&] {
        recover();
        if (manager_.dirty())
            dialog.accept();
    });
    connect(importButton, &QPushButton::clicked, &dialog, [&] {
        portableBackup(true);
        if (manager_.dirty())
            dialog.accept();
    });
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    setTextSize(QSettings().value("textSize", 10).toInt());
    return dialog.exec() == QDialog::Accepted;
}
void MainWindow::offerRecovery() {
    try {
        const auto copies = RecoveryService::copies(currentPath_);
        if (copies.isEmpty())
            return;
        bool hasAutosave = false;
        for (const auto &p : copies)
            if (p.endsWith(".recovery.clinic") || QFileInfo(p).fileName().startsWith("unsaved"))
                hasAutosave = true;
        if (hasAutosave &&
            QMessageBox::question(this, "Recovery available",
                                  "An autosave is available. Review its date and record counts "
                                  "before choosing whether to restore it?",
                                  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
            recover();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Recovery unavailable", e.what());
    }
}
void MainWindow::recover() {
    if (!confirmDiscard())
        return;
    try {
        QDialog dialog(this);
        dialog.setWindowTitle("Review recovery copies");
        dialog.resize(850, 400);
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("Review the saved time and counts. Restoring opens an unsaved "
                                     "copy; the original is retained."));
        auto *list = new QListWidget;
        layout->addWidget(list);
        auto add = [&](const QString &p) {
            QString description =
                QFileInfo(p).lastModified().toLocalTime().toString("dd MMM yyyy HH:mm:ss") + " / " +
                QFileInfo(p).fileName();
            try {
                const auto db = FileHandler::load(p);
                description += QString(" / %1 patients / %2 visits")
                                   .arg(db.patients.size())
                                   .arg(db.appointments.size());
            } catch (const std::exception &) {
                description += " / Cannot read: damaged or another Windows account";
            }
            auto *item = new QListWidgetItem(description, list);
            item->setData(Qt::UserRole, p);
            item->setToolTip(p);
        };
        for (const auto &p : RecoveryService::copies(currentPath_))
            add(p);
        if (list->count() == 0)
            layout->addWidget(
                new QLabel("No automatic copies found. Browse to a protected recovery file."));
        auto *browse = new QPushButton("Browse for a recovery file");
        layout->addWidget(browse);
        connect(browse, &QPushButton::clicked, &dialog, [&] {
            auto p = QFileDialog::getOpenFileName(&dialog, "Choose recovery copy", {},
                                                  "Clinic files (*.clinic *.json)");
            if (!p.isEmpty()) {
                add(p);
                list->setCurrentRow(list->count() - 1);
            }
        });
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::RestoreDefaults)->setText("Restore selected");
        layout->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog,
                [&] {
                    if (!list->currentItem()) {
                        QMessageBox::information(&dialog, "Select a copy",
                                                 "Select a recovery copy first.");
                        return;
                    }
                    if (openWorkspace(list->currentItem()->data(Qt::UserRole).toString())) {
                        currentPath_.clear();
                        workspaceGuard_.reset();
                        manager_.markUnsaved();
                        refresh();
                        dialog.accept();
                    }
                });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (list->count())
            list->setCurrentRow(0);
        dialog.exec();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Recovery failed", e.what());
    }
}
void MainWindow::portableBackup(bool importing) {
    if (!importing && !manager_.isAdmin()) {
        QMessageBox::warning(this, "Backup",
                             "Only administrators may export a full workspace backup.");
        return;
    }
    if (importing && !confirmDiscard())
        return;
    const auto file = importing ? QFileDialog::getOpenFileName(this, "Import portable backup", {},
                                                               "Encrypted backup (*.scapbackup)")
                                : QFileDialog::getSaveFileName(this, "Export portable backup",
                                                               "clinic-backup.scapbackup",
                                                               "Encrypted backup (*.scapbackup)");
    if (file.isEmpty())
        return;
    bool ok = false;
    auto pass =
        QInputDialog::getText(this, "Backup passphrase",
                              importing ? "Enter the backup passphrase"
                                        : "Choose a separate backup passphrase (12-256 "
                                          "characters). Keep it safely; it cannot be recovered.",
                              QLineEdit::Password, {}, &ok);
    if (!ok)
        return;
    if (!importing) {
        auto confirmation =
            QInputDialog::getText(this, "Confirm passphrase", "Repeat the backup passphrase",
                                  QLineEdit::Password, {}, &ok);
        if (!ok)
            return;
        if (pass != confirmation) {
            QMessageBox::warning(this, "Backup", "Passphrases do not match.");
            return;
        }
    }
    try {
        if (importing) {
            QFile input(file);
            if (!input.open(QIODevice::ReadOnly) || input.size() > 21 * 1024 * 1024)
                throw model::FileError("Cannot read backup or backup exceeds size limit.");
            const auto plain = security::decryptBackup(input.readAll(), pass);
            QJsonParseError error;
            const auto json = QJsonDocument::fromJson(plain, &error);
            if (error.error != QJsonParseError::NoError || !json.isObject())
                throw model::FileError("Invalid backup contents.");
            auto db = FileHandler::deserialize(json.object());
            controller::ClinicManager candidate;
            if (!db.users.empty()) {
                auto user = QInputDialog::getText(this, "Staff sign-in",
                                                  "Administrator username from the backup",
                                                  QLineEdit::Normal, {}, &ok);
                if (!ok)
                    return;
                auto secret = QInputDialog::getText(this, "Staff sign-in", "Staff password",
                                                    QLineEdit::Password, {}, &ok);
                if (!ok)
                    return;
                candidate.replace(std::move(db), user, secret);
                if (!candidate.isAdmin())
                    throw model::ValidationError(
                        "An administrator from the backup must restore it.");
            } else
                candidate.replace(std::move(db));
            prepareChange();
            manager_ = std::move(candidate);
            manager_.markUnsaved();
            currentPath_.clear();
            workspaceGuard_.reset();
            refresh();
            QMessageBox::information(this, "Backup restored",
                                     "The backup is open as an unsaved workspace. Save As to "
                                     "create a protected file on this Windows account.");
        } else {
            auto destination = file;
            if (!destination.endsWith(".scapbackup", Qt::CaseInsensitive))
                destination += ".scapbackup";
            const auto plain = QJsonDocument(FileHandler::serialize(manager_.database()))
                                   .toJson(QJsonDocument::Compact);
            const auto cipher = security::encryptBackup(plain, pass);
            QSaveFile output(destination);
            if (!output.open(QIODevice::WriteOnly) || output.write(cipher) != cipher.size() ||
                !output.commit())
                throw model::FileError("Could not save the encrypted backup.");
            manager_.recordExport("Export portable encrypted backup");
            updateTitle();
            QMessageBox::information(this, "Backup exported",
                                     "The backup can be imported on another Windows computer using "
                                     "its backup passphrase and staff administrator credentials.");
        }
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Backup failed", e.what());
    }
}
} // namespace clinic::gui
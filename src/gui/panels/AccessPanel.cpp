#include "AccessPanel.h"
namespace clinic::gui {
AccessPanel::AccessPanel(controller::ClinicManager &m, MutationHandler f,
                         std::function<void()> secureWorkspace, QWidget *parent)
    : PanelBase(m, std::move(f), parent) {
    auto *layout = panelLayout(this, "Staff access & activity",
                               "Staff roles protect application actions. Protected files and "
                               "recovery copies are tied to this Windows account.");
    state_ = new QLabel;
    state_->setWordWrap(true);
    layout->addWidget(state_);
    auto *actions = new QHBoxLayout;
    auto *enable = actionButton("Protect this workspace", "enableSecurity", true);
    auto *add = actionButton("Add staff account", "addUser");
    auto *toggle = actionButton("Enable / disable selected", "toggleUser");
    auto *password = actionButton("Change my password", "changePassword");
    for (auto *b : {enable, add, toggle, password})
        actions->addWidget(b);
    layout->addLayout(actions);
    auto *tabs = new QTabWidget;
    users_ = makeTable({"Username", "Role", "Status"}, "staffTable");
    audit_ = makeTable({"Time", "Actor", "Action"}, "auditTable");
    tabs->addTab(users_, "Staff accounts");
    tabs->addTab(audit_, "Activity history");
    layout->addWidget(tabs, 1);
    auto *note =
        new QLabel("Administrator: all actions. Clinician: care workflows and notes. Receptionist: "
                   "registration, bookings and check-in. Viewer: read-only records and reports. "
                   "Clinical notes are restricted to clinicians and administrators.");
    note->setWordWrap(true);
    layout->addWidget(note);
    connect(enable, &QPushButton::clicked, this, [secureWorkspace] { secureWorkspace(); });
    connect(add, &QPushButton::clicked, this, &AccessPanel::addAccount);
    connect(password, &QPushButton::clicked, this, &AccessPanel::changePassword);
    connect(toggle, &QPushButton::clicked, this, [this] {
        const auto name = selectedId(users_);
        bool enabled = true;
        for (const auto &u : manager_.database().users)
            if (u.username == name)
                enabled = u.active;
        mutate_([&] { manager_.setUserActive(name, !enabled); }, "Account status updated.");
    });
}
void AccessPanel::refresh() {
    const bool admin = manager_.isAdmin(), demo = manager_.demoMode();
    state_->setText(demo ? "DEMO MODE: no staff authentication. Use only fictional records until "
                           "you protect the workspace."
                         : "Signed in as " + manager_.username() + " / " +
                               security::roleName(manager_.role()) +
                               ". Workspace automatically locks after five minutes of inactivity.");
    findChild<QPushButton *>("enableSecurity")->setEnabled(demo);
    findChild<QPushButton *>("addUser")->setEnabled(admin && !demo);
    findChild<QPushButton *>("toggleUser")->setEnabled(admin && !demo);
    findChild<QPushButton *>("changePassword")->setEnabled(!demo && manager_.authenticated());
    QList<QStringList> users, audit;
    if (admin) {
        for (const auto &u : manager_.database().users)
            users.append(
                {u.username, security::roleName(u.role), u.active ? "Enabled" : "Disabled"});
        for (auto it = manager_.database().audit.rbegin(); it != manager_.database().audit.rend();
             ++it)
            audit.append(
                {it->time.toLocalTime().toString("dd MMM yyyy HH:mm:ss"), it->actor, it->action});
    }
    fillTable(users_, users);
    fillTable(audit_, audit);
}
void AccessPanel::addAccount() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add staff account");
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit;
    auto *password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    auto *role = new QComboBox;
    role->addItems({"Administrator", "Clinician", "Receptionist", "Viewer"});
    form->addRow("Username", name);
    form->addRow("Password (12+ characters)", password);
    form->addRow("Role", role);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        mutate_(
            [&] {
                manager_.addUser(name->text(), password->text(),
                                 security::parseRole(role->currentText()));
            },
            "Staff account added. Save the workspace.");
}
void AccessPanel::changePassword() {
    QDialog dialog(this);
    dialog.setWindowTitle("Change password");
    auto *form = new QFormLayout(&dialog);
    auto *current = new QLineEdit;
    current->setEchoMode(QLineEdit::Password);
    auto *replacement = new QLineEdit;
    replacement->setEchoMode(QLineEdit::Password);
    form->addRow("Current password", current);
    form->addRow("New password (12+ characters)", replacement);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        mutate_([&] { manager_.changePassword(current->text(), replacement->text()); },
                "Password changed. Save the workspace.");
}
} // namespace clinic::gui

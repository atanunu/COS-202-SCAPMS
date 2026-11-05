#pragma once
#include "controller/ClinicManager.h"
#include <QElapsedTimer>
#include <QMainWindow>
#include <functional>
#include <vector>
#include "persistence/WorkspaceGuard.h"
class QListWidget;
class QStackedWidget;
class QLabel;
class QPushButton;
class QToolBar;
class QAction;
namespace clinic::gui {
class PanelBase;
class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void loadPath(const QString &, const QString &username = {}, const QString &password = {});
    bool openWorkspace(const QString &);
    void captureScreenshots(const QString &);
    controller::ClinicManager &manager() {
        return manager_;
    }
    void refresh();
    void autosaveNow();
    bool welcome();
    void offerRecovery();

  protected:
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

  private:
    controller::ClinicManager manager_;
    std::unique_ptr<persistence::WorkspaceGuard> workspaceGuard_;
    void rememberWorkspace();
    void portableBackup(bool importing);
    void setTextSize(int);
    QString currentPath_, recoveryError_;
    QListWidget *navigation_;
    QStackedWidget *pages_;
    QLabel *fileLabel_, *sessionLabel_;
    QPushButton *undoButton_, *lockButton_;
    QAction *undoAction_;
    QToolBar *toolbar_;
    std::vector<PanelBase *> panels_;
    QElapsedTimer idle_;
    bool locking_ = false;
    void run(const std::function<void()> &, const QString &);
    void prepareChange();
    void updateTitle();
    bool save(bool choosePath = false);
    bool confirmDiscard();
    void openFile();
    void recover();
    void protectWorkspace();
    void lockWorkspace();
};
} // namespace clinic::gui

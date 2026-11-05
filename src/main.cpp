#include "gui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>
#include <cstdio>
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("MivaStudy");
    QApplication::setApplicationName("SCAPMS");
    QApplication::setApplicationVersion("3.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Smart Clinic Appointment & Patient Management System");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"d", "data"}, "Load a JSON workspace.", "file"});
    parser.addOption({"screenshots", "Capture all panels and exit.", "directory"});
    parser.process(app);
    clinic::gui::MainWindow window;
    try {
        // Native modal dialogs need a visible owner during startup.
        window.show();
        if (parser.isSet("data")) {
            if (parser.isSet("screenshots"))
                window.loadPath(parser.value("data"));
            else if (!window.openWorkspace(parser.value("data")))
                return 1;
        }
        if (!parser.isSet("data") && !parser.isSet("screenshots") && !window.welcome())
            return 0;
        if (parser.isSet("data") && !parser.isSet("screenshots"))
            window.offerRecovery();
        if (parser.isSet("screenshots"))
            QTimer::singleShot(100, &window, [&] {
                try {
                    window.captureScreenshots(parser.value("screenshots"));
                    app.exit(0);
                } catch (const std::exception &e) {
                    std::fprintf(stderr, "%s\n", e.what());
                    app.exit(1);
                }
            });
        return app.exec();
    } catch (const std::exception &e) {
        if (parser.isSet("screenshots"))
            std::fprintf(stderr, "%s\n", e.what());
        else
            QMessageBox::critical(nullptr, "SCAPMS startup error", QString::fromUtf8(e.what()));
        return 1;
    }
}

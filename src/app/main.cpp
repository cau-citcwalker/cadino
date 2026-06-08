#include <QApplication>

#include <spdlog/spdlog.h>

#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Cadino");
    QApplication::setOrganizationName("Cadino");

    spdlog::info("Cadino starting up");

    cadino::app::MainWindow window;
    window.show();

    return app.exec();
}

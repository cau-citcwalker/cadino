#include <QApplication>
#include <QSurfaceFormat>

#include <spdlog/spdlog.h>

#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Cadino");
    QApplication::setOrganizationName("Cadino");

    spdlog::info("Cadino starting up");

    cadino::app::MainWindow window;
    window.show();

    return app.exec();
}

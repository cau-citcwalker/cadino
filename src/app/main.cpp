#include <QApplication>
#include <QSurfaceFormat>
#include <QtGlobal>

#include <cstdio>
#include <string>

#include <spdlog/spdlog.h>

#include "MainWindow.hpp"

namespace {
void qt_log_handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    const char* tag = "qt";
    switch (type) {
        case QtDebugMsg:    tag = "qt-debug"; break;
        case QtInfoMsg:     tag = "qt-info"; break;
        case QtWarningMsg:  tag = "qt-warn"; break;
        case QtCriticalMsg: tag = "qt-crit"; break;
        case QtFatalMsg:    tag = "qt-fatal"; break;
    }
    const std::string where = ctx.file ? std::string(ctx.file) + ":" + std::to_string(ctx.line) : std::string("?");
    // Force-write to stderr immediately so we still see the message when Qt
    // breaks into the debugger right after.
    std::fprintf(stderr, "[%s] %s (%s)\n", tag, msg.toStdString().c_str(), where.c_str());
    std::fflush(stderr);
    spdlog::info("[{}] {} ({})", tag, msg.toStdString(), where);
    if (type == QtFatalMsg) spdlog::default_logger()->flush();
}
}  // namespace

int main(int argc, char* argv[]) {
    qInstallMessageHandler(qt_log_handler);

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

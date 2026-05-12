#include "app/appcontroller.h"
#include "core/logging/logger.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GroundStation");

    Logger::info("APP_START", "地面站客户端启动");

    AppController controller;
    if (controller.start() != 0) {
        Logger::error("APP_START_FAILED", "地面站客户端启动失败");
        return -1;
    }

    return app.exec();
}

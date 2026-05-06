#include "app/appcontroller.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("GroundStation");

    qDebug() << "=== 地面站客户端启动 ===";

    AppController controller;
    if (controller.start() != 0) {
        return -1;
    }

    return app.exec();
}

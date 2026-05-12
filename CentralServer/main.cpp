#include "core/database/databasemanager.h"
#include "ui/serverwindow.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    DatabaseManager& dbManager = DatabaseManager::instance();
    QObject::connect(&dbManager, &DatabaseManager::connectionChanged,
                     [](bool connected) {
                         qDebug() << "Database connection changed:" << connected;
                     });

    if (!dbManager.initialize()) {
        qCritical() << "数据库初始化失败，程序退出:" << dbManager.lastError();
        return -1;
    }

    ServerWindow window;
    if (!window.startServer()) {
        qCritical() << "服务器启动失败";
        return -1;
    }

    window.show();
    return app.exec();
}

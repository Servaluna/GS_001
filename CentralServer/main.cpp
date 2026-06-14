#include "core/database/databasemanager.h"
#include "core/logging/serverlogger.h"
#include "ui/serverwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ServerLogger::info("APP_START", "CentralServer 启动");

    DatabaseManager& dbManager = DatabaseManager::instance();
    QObject::connect(&dbManager, &DatabaseManager::connectionChanged,
                     [](bool connected) {
                         ServerLogger::info("DATABASE_CONNECTION_CHANGED",
                                            "数据库连接状态变化",
                                            {{"connected", connected}});
                     });

    if (!dbManager.initialize()) {
        ServerLogger::error("APP_START_FAILED",
                            "数据库初始化失败，程序退出",
                            {{"error", dbManager.lastError()}});
        return -1;
    }

    ServerWindow window;
    if (!window.startServer()) {
        ServerLogger::error("SERVER_START_FAILED", "服务器启动失败");
        return -1;
    }

    window.show();
    return app.exec();
}

#include "databasemanager.h"

#include "../logging/serverlogger.h"

#include <QSqlError>

const QString DatabaseManager::CONNECTION_NAME = "main_connection";

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject{parent}
    , m_initialized(false)
{}

DatabaseManager::~DatabaseManager()
{
    close();
}

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize()
{
    if (m_initialized) {
        ServerLogger::debug("DATABASE_ALREADY_INITIALIZED", "数据库连接已经初始化");
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        m_lastError = "QMYSQL driver not available";
        ServerLogger::error("DATABASE_DRIVER_MISSING", "QMYSQL 驱动不可用", {{"error", m_lastError}});
        emit errorOccurred(m_lastError);
        return false;
    }

    m_db = QSqlDatabase::addDatabase("QMYSQL", CONNECTION_NAME);
    m_db.setHostName("localhost");
    m_db.setPort(3306);
    m_db.setDatabaseName("centralserver");
    m_db.setUserName("root");
    m_db.setPassword("root");

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        ServerLogger::error("DATABASE_CONNECT_FAILED",
                            "数据库连接失败",
                            {{"host", "localhost"}, {"port", 3306}, {"database", "centralserver"}, {"error", m_lastError}});
        emit errorOccurred(m_lastError);
        return false;
    }

    m_initialized = true;
    ServerLogger::info("DATABASE_CONNECTED",
                       "数据库连接成功",
                       {{"host", "localhost"}, {"port", 3306}, {"database", "centralserver"}});
    emit connectionChanged(true);
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }

    if (m_initialized) {
        m_initialized = false;
        emit connectionChanged(false);
        ServerLogger::info("DATABASE_DISCONNECTED", "数据库连接已关闭");
    }
}

QSqlDatabase DatabaseManager::getDatabase() const
{
    return m_db;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

bool DatabaseManager::isConnected() const
{
    return m_initialized && m_db.isOpen();
}

#include "localdatabase.h"

#include "../logging/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

LocalDatabase* LocalDatabase::m_instance = nullptr;

namespace {

QString findGroundStationDataPath(const QString& relativePath)
{
    const auto tryFrom = [&](const QString& startPath) -> QString {
        QDir dir(startPath);
        for (int i = 0; i < 8; ++i) {
            if (QFileInfo::exists(dir.filePath("GroundStation.pro"))) {
                return QDir::cleanPath(dir.filePath(relativePath));
            }

            const QString nestedProject = dir.filePath("GroundStation/GroundStation.pro");
            if (QFileInfo::exists(nestedProject)) {
                return QDir::cleanPath(dir.filePath("GroundStation/" + relativePath));
            }

            if (!dir.cdUp()) {
                break;
            }
        }
        return QString();
    };

    QString resolved = tryFrom(QCoreApplication::applicationDirPath());
    if (!resolved.isEmpty()) {
        return resolved;
    }

    resolved = tryFrom(QDir::currentPath());
    if (!resolved.isEmpty()) {
        return resolved;
    }

    return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(relativePath));
}

QString resolveDatabasePath(const QString& dbPath)
{
    QFileInfo info(dbPath);
    if (info.isAbsolute()) {
        return QDir::cleanPath(info.absoluteFilePath());
    }

    return findGroundStationDataPath(dbPath);
}

}

LocalDatabase* LocalDatabase::getInstance()
{
    if (!m_instance) {
        m_instance = new LocalDatabase();
    }
    return m_instance;
}

void LocalDatabase::destroyInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

LocalDatabase::LocalDatabase(QObject *parent)
    : QObject(parent)
    , m_isInitialized(false)
{}

LocalDatabase::~LocalDatabase()
{
    close();
}

bool LocalDatabase::init(const QString& dbPath)
{
    if (m_isInitialized) {
        return true;
    }

    m_dbPath = resolveDatabasePath(dbPath);

    QDir dir;
    const QString dirPath = QFileInfo(m_dbPath).absolutePath();
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        m_lastError = "无法创建数据库目录: " + dirPath;
        Logger::error("DATABASE_INIT_FAILED", m_lastError, {{"db_path", m_dbPath}, {"requested_path", dbPath}});
        return false;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        Logger::error("DATABASE_OPEN_FAILED", "打开数据库失败", {{"db_path", m_dbPath}, {"requested_path", dbPath}, {"error", m_lastError}});
        return false;
    }

    m_isInitialized = true;

    if (!createTables()) {
        m_isInitialized = false;
        m_lastError = "创建表失败";
        Logger::error("DATABASE_INIT_FAILED", m_lastError, {{"db_path", m_dbPath}, {"requested_path", dbPath}});
        return false;
    }

    Logger::info("DATABASE_READY", "gs_local 数据库初始化成功", {{"db_path", m_dbPath}, {"requested_path", dbPath}});
    return true;
}

void LocalDatabase::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_isInitialized = false;
}

bool LocalDatabase::executeQuery(const QString& sql)
{
    if (!m_isInitialized) {
        Logger::warn("DATABASE_QUERY_REJECTED", "数据库未初始化");
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        Logger::error("DATABASE_EXEC_FAILED", "执行 SQL 失败", {{"sql", sql}, {"error", m_lastError}});
        return false;
    }
    return true;
}

QSqlQuery LocalDatabase::executeQueryWithResult(const QString& sql)
{
    QSqlQuery query(m_db);
    if (!m_isInitialized) {
        Logger::warn("DATABASE_QUERY_REJECTED", "数据库未初始化");
        return query;
    }

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        Logger::error("DATABASE_QUERY_FAILED", "查询 SQL 失败", {{"sql", sql}, {"error", m_lastError}});
    }
    return query;
}

bool LocalDatabase::beginTransaction()
{
    return m_isInitialized && m_db.transaction();
}

bool LocalDatabase::commitTransaction()
{
    return m_isInitialized && m_db.commit();
}

bool LocalDatabase::rollbackTransaction()
{
    return m_isInitialized && m_db.rollback();
}

bool LocalDatabase::createTables()
{
    const QString createDeviceUpgradeTaskSql = R"(
        CREATE TABLE IF NOT EXISTS device_upgrade_task (
            device_task_id INTEGER PRIMARY KEY,

            server_device_task_id INTEGER,
            aircraft_task_id INTEGER,
            batch_id INTEGER,

            owner_user_id INTEGER NOT NULL,
            assigned_operator_user_id INTEGER,

            aircraft_code TEXT,
            device_code TEXT NOT NULL,
            file_code TEXT NOT NULL,
            execution_order INTEGER DEFAULT 0,

            status INTEGER DEFAULT 0, -- 0-WAITING 1-DOWNLOADING 2-TRANSFERRING 3-INSTALLING 4-VERIFYING 5-SUCCESS 6-FAILED
            progress REAL DEFAULT 0,
            current_phase TEXT,       -- waiting/downloading/transferring/installing/verifying/success/failed
            retry_count INTEGER DEFAULT 0,

            local_package_path TEXT,

            total_size INTEGER DEFAULT 0,
            downloaded_size INTEGER DEFAULT 0,
            transferred_size INTEGER DEFAULT 0,

            start_time DATETIME,
            last_update_time DATETIME,
            finish_time DATETIME,
            last_error TEXT
        )
    )";

    const QString createDownloadSessionSql = R"(
        CREATE TABLE IF NOT EXISTS download_session (
            download_session_id TEXT PRIMARY KEY,

            device_task_id INTEGER NOT NULL,
            file_code TEXT NOT NULL,

            status INTEGER DEFAULT 0,
            progress REAL DEFAULT 0,

            local_path TEXT,
            temp_path TEXT,
            downloaded_size INTEGER DEFAULT 0,
            total_size INTEGER DEFAULT 0,
            checksum_sha256 TEXT,

            started_at DATETIME,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            finished_at DATETIME,
            expire_time DATETIME,
            error_message TEXT,

            FOREIGN KEY (device_task_id) REFERENCES device_upgrade_task(device_task_id) ON DELETE CASCADE
        )
    )";

    const QString createTransferSessionSql = R"(
        CREATE TABLE IF NOT EXISTS transfer_session (
            transfer_session_id TEXT PRIMARY KEY,

            device_task_id INTEGER NOT NULL,
            device_code TEXT NOT NULL,
            file_code TEXT NOT NULL,

            status INTEGER DEFAULT 0,
            progress REAL DEFAULT 0,

            local_package_path TEXT,
            transferred_size INTEGER DEFAULT 0,
            total_size INTEGER DEFAULT 0,
            checksum_sha256 TEXT,

            started_at DATETIME,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            finished_at DATETIME,
            error_message TEXT,

            FOREIGN KEY (device_task_id) REFERENCES device_upgrade_task(device_task_id) ON DELETE CASCADE
        )
    )";

    if (!executeQuery("PRAGMA foreign_keys = ON") ||
        !executeQuery(createDeviceUpgradeTaskSql) ||
        !executeQuery(createDownloadSessionSql) ||
        !executeQuery(createTransferSessionSql)) {
        return false;
    }

    executeQuery("CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_operator ON device_upgrade_task(assigned_operator_user_id)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_aircraft ON device_upgrade_task(aircraft_task_id)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_status ON device_upgrade_task(status)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_order ON device_upgrade_task(aircraft_task_id, execution_order)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_download_session_device_task ON download_session(device_task_id)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_download_session_status ON download_session(status)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_transfer_session_device_task ON transfer_session(device_task_id)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_transfer_session_status ON transfer_session(status)");

    return true;
}

#include "localdatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QVariant>

LocalDatabase* LocalDatabase::m_instance = nullptr;

LocalDatabase* LocalDatabase::getInstance()
{
    if (!m_instance) {
        m_instance = new LocalDatabase();
    }
    return m_instance;
}

void LocalDatabase::destroyInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
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

    m_dbPath = dbPath;

    QDir dir;
    QString dirPath = QFileInfo(dbPath).absolutePath();
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        m_lastError = "无法创建数据库目录: " + dirPath;
        qWarning() << m_lastError;
        return false;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "打开数据库失败:" << m_lastError;
        return false;
    }

    m_isInitialized = true;

    if (!createTables()) {
        m_isInitialized = false;
        m_lastError = "创建表失败";
        return false;
    }

    qDebug() << "gs_local 数据库初始化成功:" << dbPath;
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
        qWarning() << "数据库未初始化";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "执行 SQL 失败:" << sql << "错误:" << m_lastError;
        return false;
    }
    return true;
}

QSqlQuery LocalDatabase::executeQueryWithResult(const QString& sql)
{
    QSqlQuery query(m_db);
    if (!m_isInitialized) {
        qWarning() << "数据库未初始化";
        return query;
    }

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "查询 SQL 失败:" << sql << "错误:" << m_lastError;
    }
    return query;
}

bool LocalDatabase::beginTransaction()
{
    if (!m_isInitialized) return false;
    return m_db.transaction();
}

bool LocalDatabase::commitTransaction()
{
    if (!m_isInitialized) return false;
    return m_db.commit();
}

bool LocalDatabase::rollbackTransaction()
{
    if (!m_isInitialized) return false;
    return m_db.rollback();
}

bool LocalDatabase::createTables()
{
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS transferring_tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id VARCHAR(64) NOT NULL UNIQUE,
            file_id VARCHAR(64) NOT NULL,
            task_type INTEGER NOT NULL DEFAULT 0,
            description VARCHAR(255) DEFAULT '',
            target_device_id VARCHAR(64) NOT NULL,
            priority INTEGER NOT NULL DEFAULT 5,
            file_name VARCHAR(255) NOT NULL,
            file_size INTEGER NOT NULL DEFAULT 0,
            file_sha256 VARCHAR(64) NOT NULL,
            transferred_bytes INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL DEFAULT 0,
            current_step INTEGER NOT NULL DEFAULT 0,
            error_message TEXT DEFAULT '',
            create_time INTEGER NOT NULL,
            start_time INTEGER DEFAULT 0,
            end_time INTEGER DEFAULT 0,
            last_update_time INTEGER NOT NULL,
            local_cache_path VARCHAR(512) DEFAULT '',
            local_temp_path VARCHAR(512) DEFAULT ''
        )
    )";

    if (!executeQuery(createTableSql)) {
        return false;
    }

    QSqlQuery columnQuery(m_db);
    if (columnQuery.exec("PRAGMA table_info(transferring_tasks)")) {
        QSet<QString> columns;
        while (columnQuery.next()) {
            columns.insert(columnQuery.value("name").toString());
        }

        if (!columns.contains("file_sha256") &&
            !executeQuery("ALTER TABLE transferring_tasks ADD COLUMN file_sha256 VARCHAR(64) NOT NULL DEFAULT ''")) {
            return false;
        }
        if (!columns.contains("local_cache_path") &&
            !executeQuery("ALTER TABLE transferring_tasks ADD COLUMN local_cache_path VARCHAR(512) DEFAULT ''")) {
            return false;
        }
        if (!columns.contains("local_temp_path") &&
            !executeQuery("ALTER TABLE transferring_tasks ADD COLUMN local_temp_path VARCHAR(512) DEFAULT ''")) {
            return false;
        }
    }

    executeQuery("CREATE INDEX IF NOT EXISTS idx_task_id ON transferring_tasks(task_id)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_status ON transferring_tasks(status)");
    executeQuery("CREATE INDEX IF NOT EXISTS idx_priority ON transferring_tasks(priority)");

    return true;
}

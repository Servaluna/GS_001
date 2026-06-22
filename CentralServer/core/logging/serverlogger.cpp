#include "serverlogger.h"

#include "../database/databasemanager.h"

#include <QDebug>
#include <QDir>
#include <QHostInfo>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

#ifndef CENTRALSERVER_PROJECT_DIR
#define CENTRALSERVER_PROJECT_DIR ""
#endif

namespace {

QString normalizeLevel(const QString& level)
{
    const QString upper = level.trimmed().toUpper();
    if (upper == "DEBUG" || upper == "WARN" || upper == "ERROR") {
        return upper;
    }
    return "INFO";
}

QString localMachineId()
{
    const QString hostName = QHostInfo::localHostName();
    return hostName.isEmpty() ? QStringLiteral("CENTRAL-SERVER") : hostName;
}

QString centralServerProjectDir()
{
    const QString projectDir = QString::fromUtf8(CENTRALSERVER_PROJECT_DIR);
    if (!projectDir.isEmpty()) {
        return QDir::cleanPath(projectDir);
    }
    return QDir::currentPath();
}

QVariant nullableInt(int value)
{
    return value > 0 ? QVariant(value) : QVariant();
}

}

QJsonObject ServerLogEntry::toJson() const
{
    QJsonObject json;
    json["event_type"] = event_type;
    json["event_level"] = event_level;
    json["operator_user_id"] = operator_user_id > 0 ? operator_user_id : QJsonValue();
    json["client_machine_id"] = client_machine_id;
    json["session_id"] = session_id;
    json["batch_id"] = batch_id > 0 ? batch_id : QJsonValue();
    json["aircraft_task_id"] = aircraft_task_id > 0 ? aircraft_task_id : QJsonValue();
    json["device_task_id"] = device_task_id > 0 ? device_task_id : QJsonValue();
    json["aircraft_id"] = aircraft_id > 0 ? aircraft_id : QJsonValue();
    json["device_id"] = device_id > 0 ? device_id : QJsonValue();
    json["file_id"] = file_id > 0 ? file_id : QJsonValue();
    json["event_message"] = event_message;
    json["event_detail"] = event_detail;
    json["ip_address"] = ip_address;
    json["created_at"] = created_at.toString(Qt::ISODate);
    return json;
}

ServerLogger& ServerLogger::instance()
{
    static ServerLogger logger;
    return logger;
}

void ServerLogger::debug(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "DEBUG", message, detail);
}

void ServerLogger::debug(const QString& eventType,
                         const QString& message,
                         const ServerLogContext& context,
                         const QJsonObject& detail)
{
    instance().log(eventType, "DEBUG", message, context, detail);
}

void ServerLogger::info(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "INFO", message, detail);
}

void ServerLogger::info(const QString& eventType,
                        const QString& message,
                        const ServerLogContext& context,
                        const QJsonObject& detail)
{
    instance().log(eventType, "INFO", message, context, detail);
}

void ServerLogger::warn(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "WARN", message, detail);
}

void ServerLogger::warn(const QString& eventType,
                        const QString& message,
                        const ServerLogContext& context,
                        const QJsonObject& detail)
{
    instance().log(eventType, "WARN", message, context, detail);
}

void ServerLogger::error(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "ERROR", message, detail);
}

void ServerLogger::error(const QString& eventType,
                         const QString& message,
                         const ServerLogContext& context,
                         const QJsonObject& detail)
{
    instance().log(eventType, "ERROR", message, context, detail);
}

void ServerLogger::setServerMachineId(const QString& machineId)
{
    m_serverMachineId = machineId.trimmed();
}

void ServerLogger::setDatabaseLoggingEnabled(bool enabled)
{
    m_databaseLoggingEnabled = enabled;
}

void ServerLogger::log(const ServerLogEntry& entry)
{
    const ServerLogEntry normalized = withDefaults(entry);
    writeToQtLog(normalized);
    writeToFileLog(normalized);
    writeToDatabase(normalized);
    emit logGenerated(normalized);
}

void ServerLogger::log(const QString& eventType,
                       const QString& level,
                       const QString& message,
                       const QJsonObject& detail)
{
    ServerLogEntry entry;
    entry.event_type = eventType;
    entry.event_level = level;
    entry.event_message = message;
    entry.event_detail = detail;
    log(entry);
}

void ServerLogger::log(const QString& eventType,
                       const QString& level,
                       const QString& message,
                       const ServerLogContext& context,
                       const QJsonObject& detail)
{
    ServerLogEntry entry;
    entry.event_type = eventType;
    entry.event_level = level;
    entry.event_message = message;
    entry.event_detail = detail;
    entry.operator_user_id = context.operator_user_id;
    entry.client_machine_id = context.client_machine_id;
    entry.session_id = context.session_id;
    entry.batch_id = context.batch_id;
    entry.aircraft_task_id = context.aircraft_task_id;
    entry.device_task_id = context.device_task_id;
    entry.aircraft_id = context.aircraft_id;
    entry.device_id = context.device_id;
    entry.file_id = context.file_id;
    entry.ip_address = context.ip_address;
    log(entry);
}

ServerLogger::ServerLogger(QObject *parent)
    : QObject{parent}
    , m_serverMachineId(localMachineId())
{}

ServerLogEntry ServerLogger::withDefaults(const ServerLogEntry& entry) const
{
    ServerLogEntry normalized = entry;
    normalized.event_type = normalized.event_type.trimmed().isEmpty()
        ? QStringLiteral("SERVER_GENERAL")
        : normalized.event_type.trimmed().left(64);
    normalized.event_level = normalizeLevel(normalized.event_level);
    normalized.client_machine_id = normalized.client_machine_id.trimmed().isEmpty()
        ? m_serverMachineId.left(128)
        : normalized.client_machine_id.trimmed().left(128);
    normalized.session_id = normalized.session_id.trimmed().left(128);
    normalized.ip_address = normalized.ip_address.trimmed().left(64);
    normalized.created_at = normalized.created_at.isValid()
        ? normalized.created_at
        : QDateTime::currentDateTime();
    return normalized;
}

void ServerLogger::writeToQtLog(const ServerLogEntry& entry) const
{
    const QString line = formatLogLine(entry);

    if (entry.event_level == "ERROR") {
        qCritical().noquote() << line;
    } else if (entry.event_level == "WARN") {
        qWarning().noquote() << line;
    } else if (entry.event_level == "DEBUG") {
        qDebug().noquote() << line;
    } else {
        qInfo().noquote() << line;
    }
}

void ServerLogger::writeToFileLog(const ServerLogEntry& entry)
{
    const QString logFilePath = currentLogFilePath();
    if (logFilePath.isEmpty()) {
        return;
    }

    if (m_logFilePath != logFilePath) {
        if (m_logFile.isOpen()) {
            m_logFile.close();
        }
        m_logFile.setFileName(logFilePath);
        m_logFilePath = logFilePath;
    }

    if (!m_logFile.isOpen() && !m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&m_logFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream << entry.created_at.toString("yyyy-MM-dd hh:mm:ss.zzz")
           << ' '
           << formatLogLine(entry)
           << '\n';
    stream.flush();
}

void ServerLogger::writeToDatabase(const ServerLogEntry& entry)
{
    if (!m_databaseLoggingEnabled || m_writingDatabase) {
        return;
    }

    DatabaseManager& dbManager = DatabaseManager::instance();
    if (!dbManager.isConnected()) {
        return;
    }

    QSqlDatabase db = dbManager.getDatabase();
    if (!db.isOpen()) {
        return;
    }

    m_writingDatabase = true;

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO audit_log (
            event_type, event_level, operator_user_id, client_machine_id,
            session_id, batch_id, aircraft_task_id, device_task_id,
            aircraft_id, device_id, file_id, event_message, event_detail,
            ip_address, created_at
        ) VALUES (
            :event_type, :event_level, :operator_user_id, :client_machine_id,
            :session_id, :batch_id, :aircraft_task_id, :device_task_id,
            :aircraft_id, :device_id, :file_id, :event_message, :event_detail,
            :ip_address, :created_at
        )
    )");
    query.bindValue(":event_type", entry.event_type);
    query.bindValue(":event_level", entry.event_level);
    query.bindValue(":operator_user_id", nullableInt(entry.operator_user_id));
    query.bindValue(":client_machine_id", entry.client_machine_id);
    query.bindValue(":session_id", entry.session_id);
    query.bindValue(":batch_id", nullableInt(entry.batch_id));
    query.bindValue(":aircraft_task_id", nullableInt(entry.aircraft_task_id));
    query.bindValue(":device_task_id", nullableInt(entry.device_task_id));
    query.bindValue(":aircraft_id", nullableInt(entry.aircraft_id));
    query.bindValue(":device_id", nullableInt(entry.device_id));
    query.bindValue(":file_id", nullableInt(entry.file_id));
    query.bindValue(":event_message", entry.event_message);
    query.bindValue(":event_detail", QString::fromUtf8(QJsonDocument(entry.event_detail).toJson(QJsonDocument::Compact)));
    query.bindValue(":ip_address", entry.ip_address);
    query.bindValue(":created_at", entry.created_at.toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        qWarning().noquote() << QString("[WARN][AUDIT_LOG_WRITE_FAILED] %1").arg(query.lastError().text());
    }

    m_writingDatabase = false;
}

QString ServerLogger::logDirectoryPath() const
{
    return QDir::cleanPath(QDir(centralServerProjectDir()).filePath("data/logs"));
}

QString ServerLogger::currentLogFilePath() const
{
    const QString dirPath = logDirectoryPath();
    QDir dir;
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        return QString();
    }

    const QString fileName = QString("centralserver_%1.log")
        .arg(QDate::currentDate().toString("yyyyMMdd"));
    return QDir(dirPath).filePath(fileName);
}

QString ServerLogger::formatLogLine(const ServerLogEntry& entry) const
{
    const QString detail = QString::fromUtf8(
        QJsonDocument(entry.event_detail).toJson(QJsonDocument::Compact)
    );
    return QString("[%1][%2][user:%3][session:%4] %5 %6")
        .arg(entry.event_level,
             entry.event_type,
             entry.operator_user_id > 0 ? QString::number(entry.operator_user_id) : QStringLiteral("-"),
             entry.session_id.isEmpty() ? QStringLiteral("-") : entry.session_id,
             entry.event_message,
             detail);
}

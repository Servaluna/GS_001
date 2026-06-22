#include "logger.h"

#include <QDebug>
#include <QDir>
#include <QHostInfo>
#include <QJsonDocument>
#include <QTextStream>
#include <QUuid>

#ifndef GROUNDSTATION_PROJECT_DIR
#define GROUNDSTATION_PROJECT_DIR ""
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

QString generatedMachineId()
{
    const QString hostName = QHostInfo::localHostName();
    return hostName.isEmpty() ? QStringLiteral("GROUND-STATION") : hostName;
}

QString groundStationProjectDir()
{
    const QString projectDir = QString::fromUtf8(GROUNDSTATION_PROJECT_DIR);
    if (!projectDir.isEmpty()) {
        return QDir::cleanPath(projectDir);
    }
    return QDir::currentPath();
}

}

QJsonObject LogEntry::toJson() const
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

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::debug(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "DEBUG", message, detail);
}

void Logger::debug(const QString& eventType,
                   const QString& message,
                   const LogContext& context,
                   const QJsonObject& detail)
{
    instance().log(eventType, "DEBUG", message, context, detail);
}

void Logger::info(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "INFO", message, detail);
}

void Logger::info(const QString& eventType,
                  const QString& message,
                  const LogContext& context,
                  const QJsonObject& detail)
{
    instance().log(eventType, "INFO", message, context, detail);
}

void Logger::warn(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "WARN", message, detail);
}

void Logger::warn(const QString& eventType,
                  const QString& message,
                  const LogContext& context,
                  const QJsonObject& detail)
{
    instance().log(eventType, "WARN", message, context, detail);
}

void Logger::error(const QString& eventType, const QString& message, const QJsonObject& detail)
{
    instance().log(eventType, "ERROR", message, detail);
}

void Logger::error(const QString& eventType,
                   const QString& message,
                   const LogContext& context,
                   const QJsonObject& detail)
{
    instance().log(eventType, "ERROR", message, context, detail);
}

void Logger::setOperatorUserId(int userId)
{
    m_operatorUserId = userId;
}

void Logger::setClientMachineId(const QString& machineId)
{
    m_clientMachineId = machineId.trimmed();
}

void Logger::setSessionId(const QString& sessionId)
{
    m_sessionId = sessionId.trimmed();
}

void Logger::setIpAddress(const QString& ipAddress)
{
    m_ipAddress = ipAddress.trimmed();
}

QList<LogEntry> Logger::recentEntries() const
{
    return m_recentEntries;
}

void Logger::log(const LogEntry& entry)
{
    const LogEntry normalized = withDefaults(entry);
    writeToQtLog(normalized);
    writeToFileLog(normalized);
    m_recentEntries.append(normalized);
    if (m_recentEntries.size() > MAX_RECENT_ENTRY_COUNT) {
        m_recentEntries.remove(0, m_recentEntries.size() - MAX_RECENT_ENTRY_COUNT);
    }
    emit logGenerated(normalized);
}

void Logger::log(const QString& eventType,
                 const QString& level,
                 const QString& message,
                 const QJsonObject& detail)
{
    LogEntry entry;
    entry.event_type = eventType;
    entry.event_level = level;
    entry.event_message = message;
    entry.event_detail = detail;
    log(entry);
}

void Logger::log(const QString& eventType,
                 const QString& level,
                 const QString& message,
                 const LogContext& context,
                 const QJsonObject& detail)
{
    LogEntry entry;
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

Logger::Logger(QObject *parent)
    : QObject{parent}
    , m_operatorUserId(-1)
    , m_clientMachineId(generatedMachineId())
    , m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{}

LogEntry Logger::withDefaults(const LogEntry& entry) const
{
    LogEntry normalized = entry;
    normalized.event_type = normalized.event_type.trimmed().isEmpty()
        ? QStringLiteral("GENERAL")
        : normalized.event_type.trimmed().left(64);
    normalized.event_level = normalizeLevel(normalized.event_level);
    normalized.operator_user_id = normalized.operator_user_id > 0
        ? normalized.operator_user_id
        : m_operatorUserId;
    normalized.client_machine_id = normalized.client_machine_id.trimmed().isEmpty()
        ? m_clientMachineId.left(128)
        : normalized.client_machine_id.trimmed().left(128);
    normalized.session_id = normalized.session_id.trimmed().isEmpty()
        ? m_sessionId.left(128)
        : normalized.session_id.trimmed().left(128);
    normalized.ip_address = normalized.ip_address.trimmed().isEmpty()
        ? m_ipAddress.left(64)
        : normalized.ip_address.trimmed().left(64);
    normalized.created_at = normalized.created_at.isValid()
        ? normalized.created_at
        : QDateTime::currentDateTime();
    return normalized;
}

void Logger::writeToQtLog(const LogEntry& entry) const
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

void Logger::writeToFileLog(const LogEntry& entry)
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

QString Logger::logDirectoryPath() const
{
    return QDir::cleanPath(QDir(groundStationProjectDir()).filePath("data/logs"));
}

QString Logger::currentLogFilePath() const
{
    const QString dirPath = logDirectoryPath();
    QDir dir;
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        return QString();
    }

    const QString fileName = QString("groundstation_%1.log")
        .arg(QDate::currentDate().toString("yyyyMMdd"));
    return QDir(dirPath).filePath(fileName);
}

QString Logger::formatLogLine(const LogEntry& entry) const
{
    const QString detail = QString::fromUtf8(
        QJsonDocument(entry.event_detail).toJson(QJsonDocument::Compact)
    );
    return QString("[%1][%2][user:%3][session:%4] %5 %6")
        .arg(entry.event_level,
             entry.event_type,
             entry.operator_user_id > 0 ? QString::number(entry.operator_user_id) : QStringLiteral("-"),
             entry.session_id,
             entry.event_message,
             detail);
}

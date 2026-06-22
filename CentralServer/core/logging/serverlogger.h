#ifndef SERVERLOGGER_H
#define SERVERLOGGER_H

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <QString>

struct ServerLogEntry
{
    QString event_type;
    QString event_level;
    int operator_user_id = -1;
    QString client_machine_id;
    QString session_id;
    int batch_id = -1;
    int aircraft_task_id = -1;
    int device_task_id = -1;
    int aircraft_id = -1;
    int device_id = -1;
    int file_id = -1;
    QString event_message;
    QJsonObject event_detail;
    QString ip_address;
    QDateTime created_at;

    QJsonObject toJson() const;
};

struct ServerLogContext
{
    int operator_user_id = -1;
    QString client_machine_id;
    QString session_id;
    int batch_id = -1;
    int aircraft_task_id = -1;
    int device_task_id = -1;
    int aircraft_id = -1;
    int device_id = -1;
    int file_id = -1;
    QString ip_address;
};

class ServerLogger : public QObject
{
    Q_OBJECT

public:
    static ServerLogger& instance();

    static void debug(const QString& eventType,
                      const QString& message,
                      const QJsonObject& detail = QJsonObject());
    static void debug(const QString& eventType,
                      const QString& message,
                      const ServerLogContext& context,
                      const QJsonObject& detail = QJsonObject());
    static void info(const QString& eventType,
                     const QString& message,
                     const QJsonObject& detail = QJsonObject());
    static void info(const QString& eventType,
                     const QString& message,
                     const ServerLogContext& context,
                     const QJsonObject& detail = QJsonObject());
    static void warn(const QString& eventType,
                     const QString& message,
                     const QJsonObject& detail = QJsonObject());
    static void warn(const QString& eventType,
                     const QString& message,
                     const ServerLogContext& context,
                     const QJsonObject& detail = QJsonObject());
    static void error(const QString& eventType,
                      const QString& message,
                      const QJsonObject& detail = QJsonObject());
    static void error(const QString& eventType,
                      const QString& message,
                      const ServerLogContext& context,
                      const QJsonObject& detail = QJsonObject());

    void setServerMachineId(const QString& machineId);
    void setDatabaseLoggingEnabled(bool enabled);

    void log(const ServerLogEntry& entry);
    void log(const QString& eventType,
             const QString& level,
             const QString& message,
             const QJsonObject& detail = QJsonObject());
    void log(const QString& eventType,
             const QString& level,
             const QString& message,
             const ServerLogContext& context,
             const QJsonObject& detail = QJsonObject());

signals:
    void logGenerated(ServerLogEntry entry);

private:
    explicit ServerLogger(QObject *parent = nullptr);

    ServerLogEntry withDefaults(const ServerLogEntry& entry) const;
    void writeToQtLog(const ServerLogEntry& entry) const;
    void writeToFileLog(const ServerLogEntry& entry);
    void writeToDatabase(const ServerLogEntry& entry);
    QString logDirectoryPath() const;
    QString currentLogFilePath() const;
    QString formatLogLine(const ServerLogEntry& entry) const;

    QString m_serverMachineId;
    bool m_databaseLoggingEnabled = true;
    bool m_writingDatabase = false;
    mutable QFile m_logFile;
    mutable QString m_logFilePath;
};

#endif // SERVERLOGGER_H

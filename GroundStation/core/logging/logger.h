#ifndef LOGGER_H
#define LOGGER_H

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>

struct LogEntry
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

struct LogContext
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

class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger& instance();

    static void debug(const QString& eventType,
                      const QString& message,
                      const QJsonObject& detail = QJsonObject());
    static void debug(const QString& eventType,
                      const QString& message,
                      const LogContext& context,
                      const QJsonObject& detail = QJsonObject());
    static void info(const QString& eventType,
                     const QString& message,
                     const QJsonObject& detail = QJsonObject());
    static void info(const QString& eventType,
                     const QString& message,
                     const LogContext& context,
                     const QJsonObject& detail = QJsonObject());
    static void warn(const QString& eventType,
                     const QString& message,
                     const QJsonObject& detail = QJsonObject());
    static void warn(const QString& eventType,
                     const QString& message,
                     const LogContext& context,
                     const QJsonObject& detail = QJsonObject());
    static void error(const QString& eventType,
                      const QString& message,
                      const QJsonObject& detail = QJsonObject());
    static void error(const QString& eventType,
                      const QString& message,
                      const LogContext& context,
                      const QJsonObject& detail = QJsonObject());

    void setOperatorUserId(int userId);
    void setClientMachineId(const QString& machineId);
    void setSessionId(const QString& sessionId);
    void setIpAddress(const QString& ipAddress);

    void log(const LogEntry& entry);
    void log(const QString& eventType,
             const QString& level,
             const QString& message,
             const QJsonObject& detail = QJsonObject());
    void log(const QString& eventType,
             const QString& level,
             const QString& message,
             const LogContext& context,
             const QJsonObject& detail = QJsonObject());

signals:
    void logGenerated(LogEntry entry);

private:
    explicit Logger(QObject *parent = nullptr);

    LogEntry withDefaults(const LogEntry& entry) const;
    void writeToQtLog(const LogEntry& entry) const;

    int m_operatorUserId;
    QString m_clientMachineId;
    QString m_sessionId;
    QString m_ipAddress;
};

#endif // LOGGER_H

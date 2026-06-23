#ifndef AIRCRAFTLOGGER_H
#define AIRCRAFTLOGGER_H

#include <QFile>
#include <QObject>
#include <QString>

class AircraftLogger : public QObject
{
    Q_OBJECT

public:
    static AircraftLogger& instance();

    static void info(const QString& message);
    static void warn(const QString& message);
    static void error(const QString& message);

    void log(const QString& level, const QString& message);

private:
    explicit AircraftLogger(QObject *parent = nullptr);

    QString logDirectoryPath() const;
    QString currentLogFilePath() const;
    void writeToQtLog(const QString& line) const;
    void writeToFileLog(const QString& line);

    mutable QFile m_logFile;
    mutable QString m_logFilePath;
};

#endif // AIRCRAFTLOGGER_H

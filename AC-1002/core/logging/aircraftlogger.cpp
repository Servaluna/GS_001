#include "aircraftlogger.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextStream>

#ifndef AC1002_PROJECT_DIR
#define AC1002_PROJECT_DIR ""
#endif

namespace {

QString ac1002ProjectDir()
{
    const QString projectDir = QString::fromUtf8(AC1002_PROJECT_DIR);
    if (!projectDir.isEmpty()) {
        return QDir::cleanPath(projectDir);
    }
    return QDir::currentPath();
}

}

AircraftLogger& AircraftLogger::instance()
{
    static AircraftLogger logger;
    return logger;
}

void AircraftLogger::info(const QString& message)
{
    instance().log("INFO", message);
}

void AircraftLogger::warn(const QString& message)
{
    instance().log("WARN", message);
}

void AircraftLogger::error(const QString& message)
{
    instance().log("ERROR", message);
}

void AircraftLogger::log(const QString& level, const QString& message)
{
    const QString line = QString("[%1] %2").arg(level, message);
    writeToQtLog(line);
    writeToFileLog(line);
}

AircraftLogger::AircraftLogger(QObject *parent)
    : QObject(parent)
{}

QString AircraftLogger::logDirectoryPath() const
{
    return QDir::cleanPath(QDir(ac1002ProjectDir()).filePath("data/logs"));
}

QString AircraftLogger::currentLogFilePath() const
{
    const QString dirPath = logDirectoryPath();
    QDir dir;
    if (!dir.exists(dirPath) && !dir.mkpath(dirPath)) {
        return QString();
    }

    const QString fileName = QString("ac1002_%1.log")
        .arg(QDate::currentDate().toString("yyyyMMdd"));
    return QDir(dirPath).filePath(fileName);
}

void AircraftLogger::writeToQtLog(const QString& line) const
{
    qInfo().noquote() << line;
}

void AircraftLogger::writeToFileLog(const QString& line)
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
    stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
           << ' '
           << line
           << '\n';
    stream.flush();
}

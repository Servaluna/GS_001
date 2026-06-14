#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize();
    void close();

    QSqlDatabase getDatabase() const;
    QString lastError() const;
    bool isConnected() const;

signals:
    void connectionChanged(bool connected);
    void errorOccurred(const QString& error);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_db;
    QString m_lastError;
    bool m_initialized;
    static const QString CONNECTION_NAME;
};

#endif // DATABASEMANAGER_H

#ifndef LOCALDATABASE_H
#define LOCALDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>

class LocalDatabase : public QObject
{
    Q_OBJECT

public:
    static LocalDatabase* getInstance();
    static void destroyInstance();

    bool init(const QString& dbPath = "data/groundstation.db");
    void close();

    QSqlDatabase getDatabase() const { return m_db; }

    bool executeQuery(const QString& sql);
    QSqlQuery executeQueryWithResult(const QString& sql);

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    bool isInitialized() const { return m_isInitialized; }
    QString lastError() const { return m_lastError; }

private:
    explicit LocalDatabase(QObject *parent = nullptr);
    ~LocalDatabase();

    LocalDatabase(const LocalDatabase&) = delete;
    LocalDatabase& operator=(const LocalDatabase&) = delete;

    bool createTables();

    static LocalDatabase* m_instance;

    QSqlDatabase m_db;
    bool m_isInitialized;
    QString m_lastError;
    QString m_dbPath;
};

#endif // LOCALDATABASE_H

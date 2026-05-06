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

    // 初始化数据库
    bool init(const QString& dbPath = "data/groundstation.db");
    void close();

    // 获取数据库连接
    QSqlDatabase getDatabase() const { return m_db; }

    // 执行SQL
    bool executeQuery(const QString& sql);                  //返回成功失败
    QSqlQuery executeQueryWithResult(const QString& sql);   //返回数据信息

    // 事务支持
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // 检查数据库是否已初始化
    bool isInitialized() const { return m_isInitialized; }

    // 获取最后错误信息
    QString lastError() const { return m_lastError; }

private:
    explicit LocalDatabase(QObject *parent = nullptr);
    ~LocalDatabase();

    // 禁止拷贝
    LocalDatabase(const LocalDatabase&) = delete;
    LocalDatabase& operator=(const LocalDatabase&) = delete;

    // 创建表结构
    bool createTables();

    static LocalDatabase* m_instance;

    QSqlDatabase m_db;
    bool m_isInitialized;
    QString m_lastError;
    QString m_dbPath;
};

#endif // LOCALDATABASE_H

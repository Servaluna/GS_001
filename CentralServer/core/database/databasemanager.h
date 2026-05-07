#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#ifndef DEBUG_LOCATION
#define DEBUG_LOCATION qDebug().nospace()\
<< "[" << Q_FUNC_INFO\
       << " @ " << QFileInfo(__FILE__).fileName() << ":" << __LINE__ << "]"
#endif

#include <QObject>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QDir>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager& instance();

    bool initialize();                    // 初始化连接
    void close();                         // 关闭连接

    QSqlDatabase getDatabase() const;        // 获取数据库连接
    QString lastError() const;             // 获取错误信息
    bool isConnected() const;               // 检查连接状态


signals:
    void connectionChanged(bool connected);
    void errorOccurred(const QString& error);

private:
    explicit DatabaseManager(QObject *parent = nullptr);//私有构造函数防止new
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_db;
    QString m_lastError;
    bool m_initialized;
    static const QString CONNECTION_NAME;  // 固定的连接名
};

#endif // DATABASEMANAGER_H

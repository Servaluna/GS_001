#include "userdao.h"

#include "../database/databasemanager.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace {

UserInfo userInfoFromQuery(const QSqlQuery& query)
{
    UserInfo userInfo;
    userInfo.user_id = query.value("user_id").toInt();
    userInfo.username = query.value("username").toString();
    userInfo.password_hash = query.value("password_hash").toString();
    userInfo.role_id = query.value("role_id").toInt();
    userInfo.role = query.value("role").toString();
    userInfo.status = query.value("status").toInt();
    userInfo.last_login = query.value("last_login").toDateTime();
    userInfo.creator_user_id = query.value("creator_user_id").toInt();
    userInfo.created_at = query.value("created_at").toDateTime();
    userInfo.updated_at = query.value("updated_at").toDateTime();
    return userInfo;
}

QString userSelectSql()
{
    return QStringLiteral(
        "SELECT u.user_id, u.username, u.password_hash, u.role_id, r.name AS role, "
        "u.status, u.last_login, u.creator_user_id, u.created_at, u.updated_at "
        "FROM users u "
        "JOIN roles r ON u.role_id = r.id "
    );
}

}

UserInfo UserDAO::getUserById(int userId) const
{
    UserInfo userInfo;
    if (userId <= 0) {
        return userInfo;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        qCritical() << "数据库未连接";
        return userInfo;
    }

    QSqlQuery query(db);
    query.prepare(userSelectSql() + "WHERE u.user_id = :user_id");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        qCritical() << "按 ID 查询用户失败:" << query.lastError().text();
        return userInfo;
    }

    if (query.next()) {
        userInfo = userInfoFromQuery(query);
    }

    return userInfo;
}

UserInfo UserDAO::getUserByUsername(const QString& username) const
{
    UserInfo userInfo;
    if (username.isEmpty()) {
        return userInfo;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        qCritical() << "数据库未连接";
        return userInfo;
    }

    QSqlQuery query(db);
    query.prepare(userSelectSql() + "WHERE u.username = :username");
    query.bindValue(":username", username);

    if (!query.exec()) {
        qCritical() << "按用户名查询用户失败:" << query.lastError().text();
        return userInfo;
    }

    if (query.next()) {
        userInfo = userInfoFromQuery(query);
    }

    return userInfo;
}

bool UserDAO::updateLastLogin(int userId) const
{
    if (userId <= 0) {
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        qCritical() << "数据库未连接";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        qWarning() << "更新最后登录时间失败:" << query.lastError().text();
        return false;
    }

    return true;
}

#include "user.h"
#include "../databasemanager.h"

UserInfo User::authenticate(const QString& username, const QString& password)
{
    UserInfo userInfo;

    if (username.isEmpty() || password.isEmpty()) {
        qWarning() << "认证失败：用户名或密码为空";
        return userInfo;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        qCritical() << "数据库未连接";
        return userInfo;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT user_id, username, password_hash, role, status, last_login, "
        "creator_user_id, created_at, updated_at "
        "FROM users WHERE username = :username"
    );
    query.bindValue(":username", username);

    if (!query.exec()) {
        qCritical() << "查询用户失败:" << query.lastError().text();
        return userInfo;
    }

    if (!query.next()) {
        qDebug() << "用户不存在:" << username;
        return userInfo;
    }

    int status = query.value("status").toInt();
    if (status != 1) {
        qWarning() << "账户已禁用:" << username;
        return userInfo;
    }

    QString storedHash = query.value("password_hash").toString();
    QString inputHash = hashPassword(password);
    if (storedHash != inputHash) {
        qWarning() << "密码错误:" << username;
        return userInfo;
    }

    userInfo.user_id = query.value("user_id").toInt();
    userInfo.username = query.value("username").toString();
    userInfo.password_hash = storedHash;
    userInfo.role = query.value("role").toString();
    userInfo.status = status;
    userInfo.last_login = query.value("last_login").toDateTime();
    userInfo.creator_user_id = query.value("creator_user_id").toInt();
    userInfo.created_at = query.value("created_at").toDateTime();
    userInfo.updated_at = query.value("updated_at").toDateTime();

    updateLastLogin(userInfo.user_id);

    qDebug() << "用户认证成功:" << username << "角色:" << userInfo.role;

    return userInfo;
}

UserInfo User::getUserById(int userId)
{
    UserInfo userInfo;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen() || userId <= 0) {
        return userInfo;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT user_id, username, password_hash, role, status, last_login, "
        "creator_user_id, created_at, updated_at "
        "FROM users WHERE user_id = :id"
    );
    query.bindValue(":id", userId);

    if (query.exec() && query.next()) {
        userInfo.user_id = query.value("user_id").toInt();
        userInfo.username = query.value("username").toString();
        userInfo.password_hash = query.value("password_hash").toString();
        userInfo.role = query.value("role").toString();
        userInfo.status = query.value("status").toInt();
        userInfo.last_login = query.value("last_login").toDateTime();
        userInfo.creator_user_id = query.value("creator_user_id").toInt();
        userInfo.created_at = query.value("created_at").toDateTime();
        userInfo.updated_at = query.value("updated_at").toDateTime();
    }

    return userInfo;
}

UserInfo User::getUserByUsername(const QString& username)
{
    UserInfo userInfo;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen() || username.isEmpty()) {
        return userInfo;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT user_id, username, password_hash, role, status, last_login, "
        "creator_user_id, created_at, updated_at "
        "FROM users WHERE username = :username"
    );
    query.bindValue(":username", username);

    if (query.exec() && query.next()) {
        userInfo.user_id = query.value("user_id").toInt();
        userInfo.username = query.value("username").toString();
        userInfo.password_hash = query.value("password_hash").toString();
        userInfo.role = query.value("role").toString();
        userInfo.status = query.value("status").toInt();
        userInfo.last_login = query.value("last_login").toDateTime();
        userInfo.creator_user_id = query.value("creator_user_id").toInt();
        userInfo.created_at = query.value("created_at").toDateTime();
        userInfo.updated_at = query.value("updated_at").toDateTime();
    }

    return userInfo;
}

bool User::updateLastLogin(int userId)
{
    if (userId <= 0) return false;

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE user_id = :id");
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qWarning() << "更新最后登录时间失败:" << query.lastError().text();
        return false;
    }

    return true;
}

QString User::hashPassword(const QString& password)
{
    QByteArray hash = QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256
    );
    return QString(hash.toHex());
}

#include "userservice.h"

#include "../dao/userdao.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

QJsonObject LoginResult::toResponseJson() const
{
    QJsonObject responseData;
    responseData["token"] = token;
    responseData["user"] = QJsonObject{
        {"user_id", user.user_id},
        {"username", user.username},
        {"role_id", user.role_id},
        {"role", user.role},
        {"status", user.status}
    };
    return responseData;
}

UserService::UserService(UserDAO* userDao)
    : m_userDao(userDao)
{}

LoginResult UserService::login(const QString& username, const QString& password) const
{
    LoginResult result;

    if (username.isEmpty() || password.isEmpty()) {
        result.errorMessage = "用户名或密码为空";
        qWarning() << "认证失败:" << result.errorMessage;
        return result;
    }

    if (!m_userDao) {
        result.errorMessage = "用户数据访问对象未初始化";
        qCritical() << result.errorMessage;
        return result;
    }

    UserInfo userInfo = m_userDao->getUserByUsername(username);
    if (!userInfo.isValid()) {
        result.errorMessage = "用户名或密码错误";
        qDebug() << "用户不存在:" << username;
        return result;
    }

    if (!userInfo.isActive()) {
        result.errorMessage = "账户已禁用";
        qWarning() << "账户已禁用:" << username;
        return result;
    }

    const QString inputHash = hashPassword(password);
    if (userInfo.password_hash.compare(inputHash, Qt::CaseInsensitive) != 0) {
        result.errorMessage = "用户名或密码错误";
        qWarning() << "密码错误:" << username;
        return result;
    }

    m_userDao->updateLastLogin(userInfo.user_id);

    result.success = true;
    result.user = userInfo;
    result.token = createToken(userInfo);

    qDebug() << "用户认证成功:" << username << "角色:" << userInfo.role;
    return result;
}

QString UserService::hashPassword(const QString& password) const
{
    const QByteArray hash = QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256
    );
    return QString(hash.toHex());
}

QString UserService::createToken(const UserInfo& userInfo) const
{
    const QString rawToken = QString("%1_%2_%3")
                                 .arg(userInfo.user_id)
                                 .arg(userInfo.username)
                                 .arg(QDateTime::currentMSecsSinceEpoch());

    const QByteArray tokenHash = QCryptographicHash::hash(
        rawToken.toUtf8(),
        QCryptographicHash::Sha256
    ).toHex();

    return QString(tokenHash);
}

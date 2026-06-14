#include "userservice.h"

#include "../dao/userdao.h"
#include "../logging/serverlogger.h"

#include <QCryptographicHash>
#include <QDateTime>

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
        ServerLogger::warn("AUTH_LOGIN_REJECTED", result.errorMessage, {{"username", username}});
        return result;
    }

    if (!m_userDao) {
        result.errorMessage = "用户数据访问对象未初始化";
        ServerLogger::error("AUTH_LOGIN_FAILED", result.errorMessage, {{"username", username}});
        return result;
    }

    UserInfo userInfo = m_userDao->getUserByUsername(username);
    if (!userInfo.isValid()) {
        result.errorMessage = "用户名或密码错误";
        ServerLogger::warn("AUTH_LOGIN_REJECTED", "用户不存在", {{"username", username}});
        return result;
    }

    ServerLogContext context;
    context.operator_user_id = userInfo.user_id;

    if (!userInfo.isActive()) {
        result.errorMessage = "账户已禁用";
        ServerLogger::warn("AUTH_LOGIN_REJECTED",
                           "账户已禁用",
                           context,
                           {{"username", username}, {"role", userInfo.role}});
        return result;
    }

    const QString inputHash = hashPassword(password);
    if (userInfo.password_hash.compare(inputHash, Qt::CaseInsensitive) != 0) {
        result.errorMessage = "用户名或密码错误";
        ServerLogger::warn("AUTH_LOGIN_REJECTED",
                           "密码错误",
                           context,
                           {{"username", username}});
        return result;
    }

    m_userDao->updateLastLogin(userInfo.user_id);

    result.success = true;
    result.user = userInfo;
    result.token = createToken(userInfo);

    context.session_id = result.token;
    ServerLogger::info("AUTH_LOGIN_SUCCESS",
                       QString("用户认证成功: %1").arg(username),
                       context,
                       {{"username", username}, {"role", userInfo.role}, {"role_id", userInfo.role_id}});
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

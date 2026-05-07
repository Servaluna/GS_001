#ifndef USERSERVICE_H
#define USERSERVICE_H

#include "../../../Common/models.h"

#include <QJsonObject>
#include <QString>

class UserDAO;

struct LoginResult
{
    bool success = false;
    UserInfo user;
    QString token;
    QString errorMessage;

    QJsonObject toResponseJson() const;
};

class UserService
{
public:
    explicit UserService(UserDAO* userDao = nullptr);

    LoginResult login(const QString& username, const QString& password) const;

private:
    QString hashPassword(const QString& password) const;
    QString createToken(const UserInfo& userInfo) const;

    UserDAO* m_userDao;
};

#endif // USERSERVICE_H

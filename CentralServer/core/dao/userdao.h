#ifndef USERDAO_H
#define USERDAO_H

#include "../../../Common/models.h"

#include <QString>

class UserDAO
{
public:
    UserDAO() = default;

    UserInfo getUserById(int userId) const;
    UserInfo getUserByUsername(const QString& username) const;
    bool updateLastLogin(int userId) const;
};

#endif // USERDAO_H

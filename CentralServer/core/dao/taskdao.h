#ifndef TASKDAO_H
#define TASKDAO_H

#include "../../../Common/models.h"

#include <QJsonArray>

class TaskDAO
{
public:
    TaskDAO() = default;

    QJsonArray getAircraftTasksForUser(int userId, int roleId) const;
    QJsonArray getDeviceTasksForUser(int userId, int roleId) const;
    bool updateAircraftTaskStatus(const QJsonObject& statusData) const;
    bool updateDeviceTaskStatus(const QJsonObject& statusData) const;
    bool insertTaskAuditLog(const QJsonObject& statusData) const;
};

#endif // TASKDAO_H

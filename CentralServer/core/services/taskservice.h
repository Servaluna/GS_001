#ifndef TASKSERVICE_H
#define TASKSERVICE_H

#include "../../../Common/models.h"

#include <QJsonObject>

class TaskDAO;

class TaskService
{
public:
    explicit TaskService(TaskDAO* taskDao = nullptr);

    QJsonObject getCurrentUserTasks(int userId, int roleId) const;
    FileInfo getFileByCode(const QString& fileCode) const;
    bool updateTaskStatus(const QJsonObject& statusData) const;

private:
    TaskDAO* m_taskDao;
};

#endif // TASKSERVICE_H

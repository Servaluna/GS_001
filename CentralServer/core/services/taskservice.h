#ifndef TASKSERVICE_H
#define TASKSERVICE_H

#include <QJsonObject>

class TaskDAO;

class TaskService
{
public:
    explicit TaskService(TaskDAO* taskDao = nullptr);

    QJsonObject getCurrentUserTasks(int userId, int roleId) const;
    bool updateTaskStatus(const QJsonObject& statusData) const;

private:
    TaskDAO* m_taskDao;
};

#endif // TASKSERVICE_H

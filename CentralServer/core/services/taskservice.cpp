#include "taskservice.h"

#include "../dao/taskdao.h"

#include <QJsonArray>

TaskService::TaskService(TaskDAO* taskDao)
    : m_taskDao(taskDao)
{}

QJsonObject TaskService::getCurrentUserTasks(int userId, int roleId) const
{
    QJsonObject response;
    response["aircraft_tasks"] = m_taskDao ? m_taskDao->getAircraftTasksForUser(userId, roleId) : QJsonArray();
    response["device_tasks"] = m_taskDao ? m_taskDao->getDeviceTasksForUser(userId, roleId) : QJsonArray();
    return response;
}

FileInfo TaskService::getFileByCode(const QString& fileCode) const
{
    return m_taskDao ? m_taskDao->getFileByCode(fileCode) : FileInfo();
}

bool TaskService::updateTaskStatus(const QJsonObject& statusData) const
{
    if (!m_taskDao) {
        return false;
    }

    const bool aircraftOk = m_taskDao->updateAircraftTaskStatus(statusData);
    const bool deviceOk = m_taskDao->updateDeviceTaskStatus(statusData);
    m_taskDao->insertTaskAuditLog(statusData);
    return aircraftOk && deviceOk;
}

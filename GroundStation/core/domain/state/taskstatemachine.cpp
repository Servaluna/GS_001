#include "taskstatemachine.h"

#include "../../logging/logger.h"
#include "../../models/devicetask.h"
#include "../../repository/taskrepository.h"

TaskStateMachine::TaskStateMachine(QObject *parent)
    : QObject{parent}
    , m_repository(nullptr)
{}

bool TaskStateMachine::init(TaskRepository* repository)
{
    if (!repository) {
        Logger::error("TASK_STATE_MACHINE_INIT_FAILED", "TaskStateMachine 初始化失败，TaskRepository 为空");
        return false;
    }

    m_repository = repository;
    return true;
}

bool TaskStateMachine::resetForStart(int aircraftTaskId)
{
    if (!m_repository) {
        return false;
    }

    const QList<DeviceTask> deviceTasks = m_repository->getDeviceTasksByAircraftTaskId(aircraftTaskId);
    for (const DeviceTask& deviceTask : deviceTasks) {
        m_repository->updateDeviceStatus(deviceTask.device_task_id, DeviceTaskStatus::Waiting, 0.0);
        m_repository->updateDeviceProgress(deviceTask.device_task_id, 0, 0.0);
        m_repository->updateDeviceTransferProgress(deviceTask.device_task_id, 0, 0.0);
    }

    return m_repository->updateAircraftStatus(aircraftTaskId, DeviceTaskStatus::Waiting, 0.0);
}

bool TaskStateMachine::updateAircraftStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase)
{
    return m_repository && m_repository->updateAircraftStatus(aircraftTaskId, status, progress, phase);
}

bool TaskStateMachine::updateDeviceStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage)
{
    return m_repository && m_repository->updateDeviceStatus(deviceTaskId, status, progress, errorMessage);
}

bool TaskStateMachine::updateDeviceProgress(int deviceTaskId, qint64 downloadedBytes, double progress)
{
    return m_repository && m_repository->updateDeviceProgress(deviceTaskId, downloadedBytes, progress);
}

bool TaskStateMachine::markAircraftComplete(int aircraftTaskId, bool success, const QString& message)
{
    Q_UNUSED(message);

    if (!m_repository) {
        return false;
    }

    if (success) {
        return m_repository->updateAircraftStatus(aircraftTaskId, DeviceTaskStatus::Success, 100.0);
    }

    return m_repository->updateAircraftStatus(aircraftTaskId, DeviceTaskStatus::Failed, 0.0);
}

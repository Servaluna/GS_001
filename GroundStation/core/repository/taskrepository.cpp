#include "taskrepository.h"

#include <QtGlobal>

TaskRepository::TaskRepository() = default;

bool TaskRepository::saveAircraftTask(const AircraftTask& task)
{
    if (!m_aircraftTaskDao.upsert(task)) {
        return false;
    }
    m_aircraftTaskCache.insert(task.aircraft_task_id, task);
    return true;
}

bool TaskRepository::saveDeviceTask(const DeviceTask& task)
{
    if (!m_deviceTaskDao.upsert(task)) {
        return false;
    }
    m_deviceTaskCache.insert(task.device_task_id, task);
    return true;
}

bool TaskRepository::saveDownloadTask(const DownloadTask& task)
{
    if (!m_downloadCheckpointDao.upsert(task)) {
        return false;
    }
    m_downloadTaskCache.insert(task.task_uuid, task);
    return true;
}

bool TaskRepository::saveTransferSession(const TransferSession& session)
{
    if (!m_transferSessionDao.upsert(session)) {
        return false;
    }
    m_transferSessionCache.insert(session.transfer_session_id, session);
    return true;
}

AircraftTask TaskRepository::getAircraftTask(int aircraftTaskId)
{
    if (m_aircraftTaskCache.contains(aircraftTaskId)) {
        return m_aircraftTaskCache.value(aircraftTaskId);
    }

    AircraftTask task = m_aircraftTaskDao.getById(aircraftTaskId);
    if (task.isValid()) {
        m_aircraftTaskCache.insert(task.aircraft_task_id, task);
    }
    return task;
}

DeviceTask TaskRepository::getDeviceTask(int deviceTaskId)
{
    if (m_deviceTaskCache.contains(deviceTaskId)) {
        return m_deviceTaskCache.value(deviceTaskId);
    }

    DeviceTask task = m_deviceTaskDao.getById(deviceTaskId);
    if (task.isValid()) {
        m_deviceTaskCache.insert(task.device_task_id, task);
    }
    return task;
}

DownloadTask TaskRepository::getDownloadTask(const QString& taskUuid)
{
    if (m_downloadTaskCache.contains(taskUuid)) {
        return m_downloadTaskCache.value(taskUuid);
    }

    DownloadTask task = m_downloadCheckpointDao.getByTaskUuid(taskUuid);
    if (task.isValid()) {
        m_downloadTaskCache.insert(task.task_uuid, task);
    }
    return task;
}

DownloadTask TaskRepository::getDownloadTaskByDeviceTaskId(int deviceTaskId)
{
    const DownloadTask task = m_downloadCheckpointDao.getByTaskUuid(QString::number(deviceTaskId));
    if (task.isValid()) {
        m_downloadTaskCache.insert(task.task_uuid, task);
        return task;
    }
    return DownloadTask();
}

TransferSession TaskRepository::getTransferSession(const QString& sessionId)
{
    if (m_transferSessionCache.contains(sessionId)) {
        return m_transferSessionCache.value(sessionId);
    }

    TransferSession session = m_transferSessionDao.getBySessionId(sessionId);
    if (session.isValid()) {
        m_transferSessionCache.insert(session.transfer_session_id, session);
    }
    return session;
}

TransferSession TaskRepository::getTransferSessionByDeviceTaskId(int deviceTaskId)
{
    TransferSession session = m_transferSessionDao.getByDeviceTaskId(deviceTaskId);
    if (session.isValid()) {
        m_transferSessionCache.insert(session.transfer_session_id, session);
    }
    return session;
}

QList<AircraftTask> TaskRepository::getAircraftTasksForCurrentOperator(int operatorUserId)
{
    const QList<AircraftTask> tasks = m_aircraftTaskDao.getByCurrentOperator(operatorUserId);
    for (const AircraftTask& task : tasks) {
        m_aircraftTaskCache.insert(task.aircraft_task_id, task);
    }
    return tasks;
}

QList<AircraftTask> TaskRepository::getAllAircraftTasks()
{
    const QList<AircraftTask> tasks = m_aircraftTaskDao.getAll();
    for (const AircraftTask& task : tasks) {
        m_aircraftTaskCache.insert(task.aircraft_task_id, task);
    }
    return tasks;
}

QList<DeviceTask> TaskRepository::getDeviceTasksByAircraftTaskId(int aircraftTaskId)
{
    const QList<DeviceTask> tasks = m_deviceTaskDao.getByAircraftTaskId(aircraftTaskId);
    for (const DeviceTask& task : tasks) {
        m_deviceTaskCache.insert(task.device_task_id, task);
    }
    return tasks;
}

QList<DownloadTask> TaskRepository::getDownloadTasksByOwner(int ownerUserId)
{
    const QList<DownloadTask> tasks = m_downloadCheckpointDao.getByOwner(ownerUserId);
    for (const DownloadTask& task : tasks) {
        m_downloadTaskCache.insert(task.task_uuid, task);
    }
    return tasks;
}

bool TaskRepository::updateAircraftStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase)
{
    const bool ok = m_aircraftTaskDao.updateStatus(aircraftTaskId, status, progress, phase);
    if (ok) {
        m_aircraftTaskCache.remove(aircraftTaskId);
    }
    return ok;
}

bool TaskRepository::updateDeviceStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage)
{
    const bool ok = m_deviceTaskDao.updateStatus(deviceTaskId, status, progress, errorMessage);
    if (ok) {
        m_deviceTaskCache.remove(deviceTaskId);
    }
    return ok;
}

bool TaskRepository::updateDeviceProgress(int deviceTaskId, qint64 downloadedSize, double progress)
{
    const bool ok = m_deviceTaskDao.updateProgress(deviceTaskId, downloadedSize, progress);
    if (ok) {
        m_deviceTaskCache.remove(deviceTaskId);
    }
    return ok;
}

bool TaskRepository::updateDeviceTransferProgress(int deviceTaskId, qint64 transferredSize, double progress)
{
    const bool ok = m_deviceTaskDao.updateTransferProgress(deviceTaskId, transferredSize, progress);
    if (ok) {
        m_deviceTaskCache.remove(deviceTaskId);
    }
    return ok;
}

bool TaskRepository::updateDeviceLocalPackagePath(int deviceTaskId, const QString& localPackagePath)
{
    const bool ok = m_deviceTaskDao.updateLocalPackagePath(deviceTaskId, localPackagePath);
    if (ok) {
        m_deviceTaskCache.remove(deviceTaskId);
    }
    return ok;
}

bool TaskRepository::updateDownloadProgress(const QString& taskUuid,
                                            qint64 downloadedSize,
                                            const QString& checksumSha256,
                                            DownloadSessionStatus status)
{
    const bool ok = m_downloadCheckpointDao.updateProgress(taskUuid, downloadedSize, checksumSha256, status);
    if (ok) {
        m_downloadTaskCache.remove(taskUuid);
    }
    return ok;
}

bool TaskRepository::updateTransferProgress(const QString& sessionId,
                                            qint64 transferredSize,
                                            qint64 totalSize,
                                            TransferSessionStatus status,
                                            const QString& errorMessage)
{
    const bool ok = m_transferSessionDao.updateProgress(sessionId,
                                                        transferredSize,
                                                        totalSize,
                                                        status,
                                                        errorMessage);
    if (ok) {
        m_transferSessionCache.remove(sessionId);
    }
    return ok;
}

int TaskRepository::calculateAircraftProgress(int aircraftTaskId)
{
    const QList<DeviceTask> tasks = getDeviceTasksByAircraftTaskId(aircraftTaskId);
    if (tasks.isEmpty()) {
        return 0;
    }

    double totalProgress = 0.0;
    for (const DeviceTask& task : tasks) {
        totalProgress += task.progress;
    }
    return qBound(0, static_cast<int>(totalProgress / tasks.size()), 100);
}

void TaskRepository::clearCache()
{
    m_aircraftTaskCache.clear();
    m_deviceTaskCache.clear();
    m_downloadTaskCache.clear();
    m_transferSessionCache.clear();
}

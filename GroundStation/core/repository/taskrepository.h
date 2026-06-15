#ifndef TASKREPOSITORY_H
#define TASKREPOSITORY_H

#include "../dao/aircrafttaskdao.h"
#include "../dao/devicetaskdao.h"
#include "../dao/downloadcheckpointdao.h"
#include "../dao/transfersessiondao.h"
#include "../models/aircrafttask.h"
#include "../models/devicetask.h"
#include "../models/downloadtask.h"
#include "../models/transfersession.h"

#include <QHash>
#include <QList>
#include <QString>

class TaskRepository
{
public:
    TaskRepository();

    bool saveAircraftTask(const AircraftTask& task);
    bool saveDeviceTask(const DeviceTask& task);
    bool saveDownloadTask(const DownloadTask& task);
    bool saveTransferSession(const TransferSession& session);

    AircraftTask getAircraftTask(int aircraftTaskId);
    DeviceTask getDeviceTask(int deviceTaskId);
    DownloadTask getDownloadTask(const QString& taskUuid);
    DownloadTask getDownloadTaskByDeviceTaskId(int deviceTaskId);
    TransferSession getTransferSession(const QString& sessionId);
    TransferSession getTransferSessionByDeviceTaskId(int deviceTaskId);

    QList<AircraftTask> getAircraftTasksForCurrentOperator(int operatorUserId);
    QList<AircraftTask> getAllAircraftTasks();
    QList<DeviceTask> getDeviceTasksByAircraftTaskId(int aircraftTaskId);
    QList<DownloadTask> getDownloadTasksByOwner(int ownerUserId);

    bool updateAircraftStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase = QString());
    bool updateDeviceStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage = QString());
    bool updateDeviceProgress(int deviceTaskId, qint64 downloadedSize, double progress);
    bool updateDeviceTransferProgress(int deviceTaskId, qint64 transferredSize, double progress);
    bool updateDeviceLocalPackagePath(int deviceTaskId, const QString& localPackagePath);
    bool updateDownloadProgress(const QString& taskUuid,
                                qint64 downloadedSize,
                                const QString& checksumSha256,
                                DownloadSessionStatus status);
    bool updateTransferProgress(const QString& sessionId,
                                qint64 transferredSize,
                                qint64 totalSize,
                                TransferSessionStatus status,
                                const QString& errorMessage = QString());

    int calculateAircraftProgress(int aircraftTaskId);
    void clearCache();

private:
    AircraftTaskDAO m_aircraftTaskDao;
    DeviceTaskDAO m_deviceTaskDao;
    DownloadCheckpointDAO m_downloadCheckpointDao;
    TransferSessionDAO m_transferSessionDao;

    QHash<int, AircraftTask> m_aircraftTaskCache;
    QHash<int, DeviceTask> m_deviceTaskCache;
    QHash<QString, DownloadTask> m_downloadTaskCache;
    QHash<QString, TransferSession> m_transferSessionCache;
};

#endif // TASKREPOSITORY_H

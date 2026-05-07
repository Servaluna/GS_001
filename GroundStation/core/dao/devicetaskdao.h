#ifndef DEVICETASKDAO_H
#define DEVICETASKDAO_H

#include "../models/devicetask.h"

#include <QList>

class DeviceTaskDAO
{
public:
    DeviceTaskDAO() = default;

    bool upsert(const DeviceTask& task) const;
    bool remove(int deviceTaskId) const;
    DeviceTask getById(int deviceTaskId) const;
    QList<DeviceTask> getByAircraftTaskId(int aircraftTaskId) const;
    QList<DeviceTask> getByStatus(DeviceTaskStatus status) const;
    bool updateStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage = QString()) const;
    bool updateProgress(int deviceTaskId, qint64 downloadedSize, double progress) const;
    bool updateTransferProgress(int deviceTaskId, qint64 transferredSize, double progress) const;
    bool updateLocalPackagePath(int deviceTaskId, const QString& localPackagePath) const;
};

#endif // DEVICETASKDAO_H

#ifndef TASKSTATEMACHINE_H
#define TASKSTATEMACHINE_H

#include "taskstatus.h"

#include <QObject>
#include <QString>

class TaskRepository;

class TaskStateMachine : public QObject
{
    Q_OBJECT
public:
    explicit TaskStateMachine(QObject *parent = nullptr);

    bool init(TaskRepository* repository);
    bool resetForStart(int aircraftTaskId);
    bool updateAircraftStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase = QString());
    bool updateDeviceStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage = QString());
    bool updateDeviceProgress(int deviceTaskId, qint64 downloadedBytes, double progress);
    bool markAircraftComplete(int aircraftTaskId, bool success, const QString& message);

private:
    TaskRepository* m_repository;
};

#endif // TASKSTATEMACHINE_H

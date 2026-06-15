#ifndef TASKSERVICE_H
#define TASKSERVICE_H

#include "../models/aircrafttask.h"

#include <QJsonArray>
#include <QObject>
#include <QString>

class DeviceConnector;
class DownloadManager;
class TaskRepository;
class ServerConnector;
class TaskScheduler;
class TaskStateMachine;
class TransferManager;

class TaskService : public QObject
{
    Q_OBJECT
public:
    explicit TaskService(QObject *parent = nullptr);
    ~TaskService();

    bool init(ServerConnector* serverConnector, DeviceConnector* deviceConnector);
    void start();
    void stop();

    bool syncTasksForUser(int userId, int roleId, int timeoutMs = 5000);
    QList<AircraftTask> getExecutableAircraftTasksForUser(int userId, int roleId);

    bool startTask(const QString& aircraftTaskId);
    bool pauseTask(const QString& aircraftTaskId);
    bool resumeTask(const QString& aircraftTaskId);
    bool cancelTask(const QString& aircraftTaskId);

signals:
    void taskStarted(QString taskId, QString taskName);
    void taskProgressUpdated(QString taskId, QString step, int progress, qint64 speed = 0);
    void taskFinished(QString taskId, bool success, QString message);
    void queueStatusChanged(int pendingCount, bool isRunning);

private:
    TaskRepository* m_repository;
    DownloadManager* m_downloadManager;
    TransferManager* m_transferManager;
    TaskStateMachine* m_stateMachine;
    TaskScheduler* m_scheduler;
    bool m_initialized;
};

#endif // TASKSERVICE_H

#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include "../../models/aircrafttask.h"
#include "../../models/devicetask.h"
#include "../../models/downloadtask.h"
#include "../../models/transfersession.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QTimer>

class DownloadManager;
class TaskRepository;
class TaskStateMachine;
class TransferManager;

class TaskScheduler : public QObject
{
    Q_OBJECT
public:
    explicit TaskScheduler(QObject *parent = nullptr);
    ~TaskScheduler();

    bool init(TaskRepository* repository,
              DownloadManager* downloadManager,
              TransferManager* transferManager,
              TaskStateMachine* stateMachine);

    void start();
    void stop();

    bool startTask(const QString& aircraftTaskId);
    bool pauseTask(const QString& aircraftTaskId);
    bool resumeTask(const QString& aircraftTaskId);
    bool cancelTask(const QString& aircraftTaskId);

    void pauseCurrentTask();
    void resumeCurrentTask();

    AircraftTask getCurrentTask() const;
    int getPendingCount() const;
    bool isBusy() const;

signals:
    void taskStarted(QString taskId, QString taskName);
    void taskProgressUpdated(QString taskId, QString step, int progress, qint64 speed = 0);
    void taskFinished(QString taskId, bool success, QString message);
    void queueStatusChanged(int pendingCount, bool isRunning);

private slots:
    void onDownloadProgress(QString taskUuid, qint64 transferred, qint64 total, int progressPercent);
    void onDownloadFinished(QString taskUuid, const QString& localPath, bool success);
    void onDownloadFailed(QString taskUuid, int errorCode, const QString& errorMessage);
    void onTransferProgress(QString taskId, qint64 sent, qint64 total, int percent);
    void onDeviceSendFinished(QString taskId, bool success, const QString& message);
    void onDeviceInstallResult(QString taskId, bool success, const QString& message);
    void onProcessNextTask();
    void onUpdateDownloadSpeed();

private:
    void processNextTask();
    void processNextDeviceTask();
    void startDownloadTask(const DeviceTask& deviceTask);
    void startSendToDevice(const QString& localPath);
    void completeCurrentAircraftTask(bool success, const QString& message);
    void reportTaskStatus(DeviceTaskStatus aircraftStatus,
                          double aircraftProgress,
                          const QString& phase,
                          const DeviceTask* deviceTask = nullptr,
                          DeviceTaskStatus deviceStatus = DeviceTaskStatus::Waiting,
                          double deviceProgress = 0.0,
                          const QString& message = QString());
    DownloadTask ensureDownloadTask(const DeviceTask& deviceTask);
    TransferSession ensureTransferSession(const DeviceTask& deviceTask,
                                          const QString& localPath,
                                          const QString& checksumSha256);
    QString aircraftTaskKey() const;
    QString deviceTaskKey(const DeviceTask& deviceTask) const;
    void calculateSpeed(qint64 bytesTransferred);
    void clearCurrentTask();

    TaskRepository* m_repository;
    DownloadManager* m_downloadManager;
    TransferManager* m_transferManager;
    TaskStateMachine* m_stateMachine;

    QQueue<int> m_taskQueue;
    AircraftTask m_currentAircraftTask;
    QList<DeviceTask> m_currentDeviceTasks;
    int m_currentDeviceIndex;
    DeviceTask m_currentDeviceTask;
    QString m_currentDownloadUuid;
    QString m_currentTransferSessionId;

    QTimer* m_processTimer;
    bool m_isRunning;
    bool m_initialized;

    int m_retryCount;
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr int RETRY_DELAY_MS = 2000;

    QElapsedTimer m_speedTimer;
    qint64 m_lastTransferredBytes;
    qint64 m_currentSpeed;
    QTimer* m_speedUpdateTimer;
};

#endif // TASKSCHEDULER_H

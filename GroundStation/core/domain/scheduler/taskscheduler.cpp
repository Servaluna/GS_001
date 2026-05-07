#include "taskscheduler.h"

#include "../download/downloadmanager.h"
#include "../state/taskstatemachine.h"
#include "../transfer/transfermanager.h"
#include "../../models/downloadtask.h"
#include "../../models/transfersession.h"
#include "../../repository/taskrepository.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

constexpr int PROCESS_INTERVAL_MS = 500;
constexpr int SPEED_UPDATE_INTERVAL_MS = 1000;

TaskScheduler::TaskScheduler(QObject *parent)
    : QObject{parent}
    , m_repository(nullptr)
    , m_downloadManager(nullptr)
    , m_transferManager(nullptr)
    , m_stateMachine(nullptr)
    , m_currentDeviceIndex(-1)
    , m_processTimer(nullptr)
    , m_isRunning(false)
    , m_initialized(false)
    , m_retryCount(0)
    , m_lastTransferredBytes(0)
    , m_currentSpeed(0)
    , m_speedUpdateTimer(nullptr)
{}

TaskScheduler::~TaskScheduler()
{
    stop();
}

bool TaskScheduler::init(TaskRepository* repository,
                         DownloadManager* downloadManager,
                         TransferManager* transferManager,
                         TaskStateMachine* stateMachine)
{
    if (!repository || !downloadManager || !transferManager || !stateMachine) {
        qCritical() << "TaskScheduler::init - invalid dependency";
        return false;
    }

    m_repository = repository;
    m_downloadManager = downloadManager;
    m_transferManager = transferManager;
    m_stateMachine = stateMachine;

    connect(m_downloadManager, &DownloadManager::progressUpdated,
            this, &TaskScheduler::onDownloadProgress);
    connect(m_downloadManager, &DownloadManager::downloadFinished,
            this, &TaskScheduler::onDownloadFinished);
    connect(m_downloadManager, &DownloadManager::downloadFailed,
            this, &TaskScheduler::onDownloadFailed);

    connect(m_transferManager, &TransferManager::sendFinished,
            this, &TaskScheduler::onDeviceSendFinished);
    connect(m_transferManager, &TransferManager::transferProgress,
            this, &TaskScheduler::onTransferProgress);
    connect(m_transferManager, &TransferManager::installResult,
            this, [this](const QString& taskId, const QString&, bool success, const QString& message) {
                onDeviceInstallResult(taskId, success, message);
            });

    m_processTimer = new QTimer(this);
    connect(m_processTimer, &QTimer::timeout, this, &TaskScheduler::onProcessNextTask);

    m_speedUpdateTimer = new QTimer(this);
    connect(m_speedUpdateTimer, &QTimer::timeout, this, &TaskScheduler::onUpdateDownloadSpeed);

    m_initialized = true;
    return true;
}

void TaskScheduler::start()
{
    if (!m_initialized || m_isRunning) {
        return;
    }

    m_isRunning = true;
    m_processTimer->start(PROCESS_INTERVAL_MS);
    emit queueStatusChanged(m_taskQueue.size(), m_currentAircraftTask.isValid());
}

void TaskScheduler::stop()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    if (m_processTimer) {
        m_processTimer->stop();
    }
    if (m_speedUpdateTimer) {
        m_speedUpdateTimer->stop();
    }

    if (!m_currentDownloadUuid.isEmpty()) {
        m_downloadManager->pauseDownload(m_currentDownloadUuid);
    }
}

bool TaskScheduler::startTask(const QString& aircraftTaskId)
{
    if (!m_initialized) {
        qWarning() << "TaskScheduler 尚未初始化";
        return false;
    }

    bool ok = false;
    const int id = aircraftTaskId.toInt(&ok);
    if (!ok || id <= 0) {
        qWarning() << "飞机任务 ID 无效:" << aircraftTaskId;
        return false;
    }

    const AircraftTask task = m_repository->getAircraftTask(id);
    if (!task.isValid()) {
        qWarning() << "飞机任务不存在:" << aircraftTaskId;
        return false;
    }

    if (task.status == DeviceTaskStatus::Success) {
        qWarning() << "飞机任务已完成:" << aircraftTaskId;
        return false;
    }

    if (TaskStatusText::isRunning(task.status)) {
        qWarning() << "飞机任务正在执行:" << aircraftTaskId;
        return false;
    }

    const QList<DeviceTask> deviceTasks = m_repository->getDeviceTasksByAircraftTaskId(id);
    if (deviceTasks.isEmpty()) {
        qWarning() << "飞机任务没有设备子任务:" << aircraftTaskId;
        return false;
    }

    if (!m_stateMachine->resetForStart(id)) {
        return false;
    }

    if (!m_taskQueue.contains(id)) {
        m_taskQueue.enqueue(id);
    }

    emit queueStatusChanged(m_taskQueue.size(), m_currentAircraftTask.isValid());
    processNextTask();
    return true;
}

bool TaskScheduler::pauseTask(const QString& aircraftTaskId)
{
    if (!m_currentAircraftTask.isValid() || aircraftTaskKey() != aircraftTaskId) {
        return false;
    }

    pauseCurrentTask();
    return true;
}

bool TaskScheduler::resumeTask(const QString& aircraftTaskId)
{
    if (!m_currentAircraftTask.isValid() || aircraftTaskKey() != aircraftTaskId) {
        return false;
    }

    resumeCurrentTask();
    return true;
}

bool TaskScheduler::cancelTask(const QString& aircraftTaskId)
{
    bool ok = false;
    const int id = aircraftTaskId.toInt(&ok);
    if (!ok) {
        return false;
    }

    if (m_currentAircraftTask.isValid() && m_currentAircraftTask.aircraft_task_id == id) {
        if (!m_currentDownloadUuid.isEmpty()) {
            m_downloadManager->cancelDownload(m_currentDownloadUuid);
        }

        m_stateMachine->updateAircraftStatus(id, DeviceTaskStatus::Failed, m_currentAircraftTask.progress, "用户取消");
        clearCurrentTask();
        emit taskFinished(aircraftTaskId, false, "任务已取消");
    } else {
        for (int i = 0; i < m_taskQueue.size(); ++i) {
            if (m_taskQueue[i] == id) {
                m_taskQueue.removeAt(i);
                m_stateMachine->updateAircraftStatus(id, DeviceTaskStatus::Failed, 0.0, "用户取消");
                break;
            }
        }
    }

    emit queueStatusChanged(m_taskQueue.size(), m_currentAircraftTask.isValid());
    processNextTask();
    return true;
}

void TaskScheduler::pauseCurrentTask()
{
    if (!m_currentDownloadUuid.isEmpty()) {
        m_downloadManager->pauseDownload(m_currentDownloadUuid);
    }
}

void TaskScheduler::resumeCurrentTask()
{
    if (!m_currentDownloadUuid.isEmpty()) {
        m_downloadManager->resumeDownload(m_currentDownloadUuid);
    }
}

AircraftTask TaskScheduler::getCurrentTask() const
{
    return m_currentAircraftTask;
}

int TaskScheduler::getPendingCount() const
{
    return m_taskQueue.size();
}

bool TaskScheduler::isBusy() const
{
    return m_currentAircraftTask.isValid();
}

void TaskScheduler::onDownloadProgress(QString taskUuid, qint64 transferred, qint64 total, int progressPercent)
{
    Q_UNUSED(total);
    if (taskUuid != m_currentDownloadUuid || !m_currentDeviceTask.isValid()) {
        return;
    }

    calculateSpeed(transferred);
    const int aircraftProgress = m_repository->calculateAircraftProgress(m_currentAircraftTask.aircraft_task_id);
    m_stateMachine->updateAircraftStatus(m_currentAircraftTask.aircraft_task_id,
                                         DeviceTaskStatus::Downloading,
                                         aircraftProgress,
                                         QString("下载设备 %1").arg(m_currentDeviceTask.device_code));
    emit taskProgressUpdated(aircraftTaskKey(), "下载中", progressPercent, m_currentSpeed);
}

void TaskScheduler::onDownloadFinished(QString taskUuid, const QString& localPath, bool success)
{
    if (taskUuid != m_currentDownloadUuid || !m_currentDeviceTask.isValid()) {
        return;
    }

    if (!success) {
        onDownloadFailed(taskUuid, 1002, "文件 SHA-256 校验失败");
        return;
    }

    if (m_speedUpdateTimer) {
        m_speedUpdateTimer->stop();
    }

    startSendToDevice(localPath);
}

void TaskScheduler::onDownloadFailed(QString taskUuid, int errorCode, const QString& errorMessage)
{
    Q_UNUSED(errorCode);
    if (taskUuid != m_currentDownloadUuid || !m_currentDeviceTask.isValid()) {
        return;
    }

    if (m_speedUpdateTimer) {
        m_speedUpdateTimer->stop();
    }

    if (m_retryCount < MAX_RETRY_COUNT) {
        ++m_retryCount;
        QTimer::singleShot(RETRY_DELAY_MS, this, [this]() {
            if (m_currentDeviceTask.isValid()) {
                startDownloadTask(m_currentDeviceTask);
            }
        });
        return;
    }

    m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id,
                                       DeviceTaskStatus::Failed,
                                       m_currentDeviceTask.progress,
                                       QString("下载失败(重试%1次): %2").arg(MAX_RETRY_COUNT).arg(errorMessage));
    completeCurrentAircraftTask(false, QString("设备 %1 下载失败: %2").arg(m_currentDeviceTask.device_code, errorMessage));
}

void TaskScheduler::onTransferProgress(QString taskId, qint64 sent, qint64 total, int percent)
{
    if (taskId != deviceTaskKey(m_currentDeviceTask) || !m_currentDeviceTask.isValid()) {
        return;
    }

    const double deviceProgress = 70.0 + percent * 0.2;
    m_repository->updateTransferProgress(m_currentTransferSessionId, sent, total, TransferSessionStatus::Transferring);
    m_repository->updateDeviceTransferProgress(m_currentDeviceTask.device_task_id, sent, deviceProgress);
    emit taskProgressUpdated(aircraftTaskKey(), "发送中", static_cast<int>(deviceProgress));
}

void TaskScheduler::onDeviceSendFinished(QString taskId, bool success, const QString& message)
{
    if (taskId != deviceTaskKey(m_currentDeviceTask) || !m_currentDeviceTask.isValid()) {
        return;
    }

    if (!success) {
        m_repository->updateTransferProgress(m_currentTransferSessionId,
                                             m_currentDeviceTask.transferred_size,
                                             m_currentDeviceTask.total_size,
                                             TransferSessionStatus::Failed,
                                             message);
        m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id,
                                           DeviceTaskStatus::Failed,
                                           m_currentDeviceTask.progress,
                                           QString("发送到设备失败: %1").arg(message));
        completeCurrentAircraftTask(false, QString("设备 %1 发送失败: %2").arg(m_currentDeviceTask.device_code, message));
        return;
    }

    m_repository->updateTransferProgress(m_currentTransferSessionId,
                                         m_currentDeviceTask.total_size,
                                         m_currentDeviceTask.total_size,
                                         TransferSessionStatus::Finished);
    m_repository->updateDeviceTransferProgress(m_currentDeviceTask.device_task_id,
                                               m_currentDeviceTask.total_size,
                                               90.0);
    m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id, DeviceTaskStatus::Installing, 90.0);
    m_stateMachine->updateAircraftStatus(m_currentAircraftTask.aircraft_task_id,
                                         DeviceTaskStatus::Installing,
                                         m_repository->calculateAircraftProgress(m_currentAircraftTask.aircraft_task_id),
                                         QString("安装设备 %1").arg(m_currentDeviceTask.device_code));
    emit taskProgressUpdated(aircraftTaskKey(), "安装中", 90);
}

void TaskScheduler::onDeviceInstallResult(QString taskId, bool success, const QString& message)
{
    if (taskId != deviceTaskKey(m_currentDeviceTask) || !m_currentDeviceTask.isValid()) {
        return;
    }

    if (!success) {
        m_repository->updateTransferProgress(m_currentTransferSessionId,
                                             m_currentDeviceTask.total_size,
                                             m_currentDeviceTask.total_size,
                                             TransferSessionStatus::Failed,
                                             message);
        m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id,
                                           DeviceTaskStatus::Failed,
                                           90.0,
                                           QString("设备安装失败: %1").arg(message));
        completeCurrentAircraftTask(false, QString("设备 %1 安装失败: %2").arg(m_currentDeviceTask.device_code, message));
        return;
    }

    m_repository->updateTransferProgress(m_currentTransferSessionId,
                                         m_currentDeviceTask.total_size,
                                         m_currentDeviceTask.total_size,
                                         TransferSessionStatus::Finished);
    m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id, DeviceTaskStatus::Success, 100.0);
    ++m_currentDeviceIndex;
    processNextDeviceTask();
}

void TaskScheduler::onProcessNextTask()
{
    if (m_isRunning && !m_currentAircraftTask.isValid()) {
        processNextTask();
    }
}

void TaskScheduler::onUpdateDownloadSpeed()
{
    if (!m_currentDeviceTask.isValid()) {
        return;
    }

    const DeviceTask task = m_repository->getDeviceTask(m_currentDeviceTask.device_task_id);
    if (task.isValid()) {
        emit taskProgressUpdated(aircraftTaskKey(), "下载中", static_cast<int>(task.progress), m_currentSpeed);
    }
}

void TaskScheduler::processNextTask()
{
    if (!m_isRunning || m_currentAircraftTask.isValid() || m_taskQueue.isEmpty()) {
        return;
    }

    const int aircraftTaskId = m_taskQueue.dequeue();
    m_currentAircraftTask = m_repository->getAircraftTask(aircraftTaskId);
    if (!m_currentAircraftTask.isValid()) {
        processNextTask();
        return;
    }

    m_currentDeviceTasks = m_repository->getDeviceTasksByAircraftTaskId(aircraftTaskId);
    m_currentDeviceIndex = 0;
    m_retryCount = 0;

    m_stateMachine->updateAircraftStatus(aircraftTaskId, DeviceTaskStatus::Downloading, 0.0, "执行中");
    emit taskStarted(aircraftTaskKey(), QString("飞机 %1 升级任务").arg(m_currentAircraftTask.aircraft_code));
    emit queueStatusChanged(m_taskQueue.size(), true);

    processNextDeviceTask();
}

void TaskScheduler::processNextDeviceTask()
{
    if (!m_currentAircraftTask.isValid()) {
        processNextTask();
        return;
    }

    if (m_currentDeviceIndex >= m_currentDeviceTasks.size()) {
        completeCurrentAircraftTask(true, "任务执行完成");
        return;
    }

    m_currentDeviceTask = m_repository->getDeviceTask(m_currentDeviceTasks[m_currentDeviceIndex].device_task_id);
    m_retryCount = 0;
    startDownloadTask(m_currentDeviceTask);
}

void TaskScheduler::startDownloadTask(const DeviceTask& deviceTask)
{
    DownloadTask downloadTask = ensureDownloadTask(deviceTask);
    if (!downloadTask.isValid()) {
        completeCurrentAircraftTask(false, QString("无法创建设备 %1 的下载任务").arg(deviceTask.device_code));
        return;
    }

    m_currentDownloadUuid = downloadTask.task_uuid;
    m_stateMachine->updateDeviceStatus(deviceTask.device_task_id, DeviceTaskStatus::Downloading, 0.0);
    emit taskProgressUpdated(aircraftTaskKey(), "下载中", 0);

    m_lastTransferredBytes = downloadTask.downloaded_size;
    m_speedTimer.start();
    m_speedUpdateTimer->start(SPEED_UPDATE_INTERVAL_MS);

    if (!m_downloadManager->startDownload(deviceTask, downloadTask)) {
        onDownloadFailed(downloadTask.task_uuid, 1000, "无法启动下载");
    }
}

void TaskScheduler::startSendToDevice(const QString& localPath)
{
    m_currentDownloadUuid.clear();
    m_stateMachine->updateDeviceStatus(m_currentDeviceTask.device_task_id, DeviceTaskStatus::Transferring, 80.0);
    emit taskProgressUpdated(aircraftTaskKey(), "发送中", 80);

    const QString fileName = QFileInfo(localPath).fileName();
    const DownloadTask downloadTask = m_repository->getDownloadTaskByDeviceTaskId(m_currentDeviceTask.device_task_id);
    TransferSession transferSession = ensureTransferSession(m_currentDeviceTask,
                                                            localPath,
                                                            downloadTask.checksum_sha256);
    if (!transferSession.isValid()) {
        onDeviceSendFinished(deviceTaskKey(m_currentDeviceTask), false, "无法创建传输会话");
        return;
    }

    m_currentTransferSessionId = transferSession.transfer_session_id;
    if (!m_transferManager->isDeviceOnline(m_currentDeviceTask.device_code)) {
        m_repository->updateTransferProgress(m_currentTransferSessionId,
                                             0,
                                             m_currentDeviceTask.total_size,
                                             TransferSessionStatus::Failed,
                                             "设备离线，请检查设备连接");
        onDeviceSendFinished(deviceTaskKey(m_currentDeviceTask), false, "设备离线，请检查设备连接");
        return;
    }

    m_repository->updateTransferProgress(m_currentTransferSessionId, 0, m_currentDeviceTask.total_size, TransferSessionStatus::Transferring);
    m_transferManager->sendFileToDevice(deviceTaskKey(m_currentDeviceTask),
                                        m_currentDeviceTask.device_code,
                                        localPath,
                                        fileName,
                                        downloadTask.checksum_sha256);
}

void TaskScheduler::completeCurrentAircraftTask(bool success, const QString& message)
{
    const QString taskId = aircraftTaskKey();
    m_stateMachine->markAircraftComplete(m_currentAircraftTask.aircraft_task_id, success, message);
    emit taskFinished(taskId, success, message);
    clearCurrentTask();
    processNextTask();
}

DownloadTask TaskScheduler::ensureDownloadTask(const DeviceTask& deviceTask)
{
    DownloadTask task = m_repository->getDownloadTaskByDeviceTaskId(deviceTask.device_task_id);
    if (task.isValid()) {
        return task;
    }

    task.task_uuid = QString::number(deviceTask.device_task_id);
    task.device_task_id = deviceTask.device_task_id;
    task.file_code = deviceTask.file_code;
    task.local_path = QDir::currentPath()
        + QString("/cache/files/%1/%2.bin").arg(m_currentAircraftTask.aircraft_task_id).arg(deviceTask.file_code);
    task.temp_path = task.local_path + ".part";
    task.downloaded_size = deviceTask.downloaded_size;
    task.total_size = deviceTask.total_size;
    task.checksum_sha256.clear();
    task.created_at = QDateTime::currentDateTime();
    task.updated_at = QDateTime::currentDateTime();
    task.status = DownloadSessionStatus::Pending;

    return m_repository->saveDownloadTask(task) ? task : DownloadTask();
}

TransferSession TaskScheduler::ensureTransferSession(const DeviceTask& deviceTask,
                                                     const QString& localPath,
                                                     const QString& checksumSha256)
{
    TransferSession session = m_repository->getTransferSessionByDeviceTaskId(deviceTask.device_task_id);
    if (session.isValid()) {
        session.status = TransferSessionStatus::Pending;
        session.progress = 0.0;
        session.transferred_size = 0;
        session.total_size = deviceTask.total_size;
        session.local_package_path = localPath;
        session.checksum_sha256 = checksumSha256;
        session.updated_at = QDateTime::currentDateTime();
        return m_repository->saveTransferSession(session) ? session : TransferSession();
    }

    session.transfer_session_id = deviceTaskKey(deviceTask);
    session.device_task_id = deviceTask.device_task_id;
    session.device_code = deviceTask.device_code;
    session.file_code = deviceTask.file_code;
    session.status = TransferSessionStatus::Pending;
    session.progress = 0.0;
    session.local_package_path = localPath;
    session.transferred_size = 0;
    session.total_size = deviceTask.total_size;
    session.checksum_sha256 = checksumSha256;
    session.started_at = QDateTime::currentDateTime();
    session.updated_at = QDateTime::currentDateTime();

    return m_repository->saveTransferSession(session) ? session : TransferSession();
}

QString TaskScheduler::aircraftTaskKey() const
{
    return QString::number(m_currentAircraftTask.aircraft_task_id);
}

QString TaskScheduler::deviceTaskKey(const DeviceTask& deviceTask) const
{
    return QString::number(deviceTask.device_task_id);
}

void TaskScheduler::calculateSpeed(qint64 bytesTransferred)
{
    const qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed >= 1000) {
        const qint64 bytesDiff = bytesTransferred - m_lastTransferredBytes;
        m_currentSpeed = bytesDiff * 1000 / elapsed;
        m_lastTransferredBytes = bytesTransferred;
        m_speedTimer.restart();
    }
}

void TaskScheduler::clearCurrentTask()
{
    m_currentAircraftTask = AircraftTask();
    m_currentDeviceTasks.clear();
    m_currentDeviceIndex = -1;
    m_currentDeviceTask = DeviceTask();
    m_currentDownloadUuid.clear();
    m_currentTransferSessionId.clear();
    m_retryCount = 0;
    emit queueStatusChanged(m_taskQueue.size(), false);
}

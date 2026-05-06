#include "taskexecutor.h"
#include "filetransfermanager.h"
#include "../network/deviceconnector.h"
#include "../localdatabase/localdao.h"

// 甯搁噺瀹氫箟
constexpr int PROCESS_INTERVAL_MS = 500;   // 浠诲姟澶勭悊闂撮殧
constexpr int SPEED_UPDATE_INTERVAL_MS = 1000; // 閫熷害鏇存柊闂撮殧

TaskExecutor::TaskExecutor(QObject *parent)
    : QObject{parent}
    , m_dao(nullptr)
    , m_fileManager(nullptr)
    , m_deviceConnector(nullptr)
    , m_currentStep(CurrentSteps::Idle)
    , m_processTimer(nullptr)
    , m_isRunning(false)
    , m_initialized(false)
    , m_retryCount(0)
    , m_lastTransferredBytes(0)
    , m_currentSpeed(0)
    , m_speedUpdateTimer(nullptr)
{}

TaskExecutor::~TaskExecutor()
{
    stop();
}

bool TaskExecutor::init(LocalDAO* dao,
                        FileTransferManager* fileTransferManager,
                        DeviceConnector* deviceConnector)
{
    if (!dao || !fileTransferManager || !deviceConnector) {
        qCritical() << "TaskExecutor::init - Invalid parameters";
        return false;
    }

    m_dao = dao;
    m_fileManager = fileTransferManager;
    m_deviceConnector = deviceConnector;

    // 杩炴帴鏂囦欢绠＄悊鍣ㄤ俊鍙?
    connect(m_fileManager, &FileTransferManager::progressUpdated,
            this, &TaskExecutor::onDownloadProgress);
    connect(m_fileManager, &FileTransferManager::downloadFinished,
            this, &TaskExecutor::onDownloadFinished);
    connect(m_fileManager, &FileTransferManager::downloadFailed,
            this, &TaskExecutor::onDownloadFailed);

    // 杩炴帴璁惧杩炴帴鍣ㄤ俊鍙?
    connect(m_deviceConnector, &DeviceConnector::sendFinished,
            this, &TaskExecutor::onDeviceSendFinished);
    connect(m_deviceConnector, &DeviceConnector::installResult,
            this, [this](const QString& taskId, const QString&, bool success, const QString& message) {
                onDeviceInstallResult(taskId, success, message);
            });

    // 鍒涘缓澶勭悊瀹氭椂鍣?
    m_processTimer = new QTimer(this);
    connect(m_processTimer, &QTimer::timeout, this, &TaskExecutor::onProcessNextTask);

    // 鍒涘缓閫熷害鏇存柊瀹氭椂鍣?
    m_speedUpdateTimer = new QTimer(this);
    connect(m_speedUpdateTimer, &QTimer::timeout, this, &TaskExecutor::onUpdateDownloadSpeed);

    m_initialized = true;
    qDebug() << "TaskExecutor initialized successfully";
    return true;
}

void TaskExecutor::start()
{
    if (!m_initialized) {
        qWarning() << "TaskExecutor not initialized, call init() first";
        return;
    }

    if (m_isRunning) {
        qDebug() << "TaskExecutor already running";
        return;
    }

    m_isRunning = true;

    // 鍔犺浇鍙仮澶嶇殑浠诲姟锛堢▼搴忓紓甯搁€€鍑哄悗鎭㈠锛?
    loadResumableTasks();

    // 鍚姩澶勭悊瀹氭椂鍣?
    m_processTimer->start(PROCESS_INTERVAL_MS);

    qDebug() << "TaskExecutor started, pending tasks:" << m_taskQueue.size();
    emit queueStatusChanged(m_taskQueue.size(), m_currentTask.isValid());
}

void TaskExecutor::stop()
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

    // 濡傛灉鏈夋鍦ㄤ笅杞界殑浠诲姟锛屾殏鍋滀絾涓嶅彇娑堬紙浠ヤ究涓嬫鎭㈠锛?
    if (m_currentTask.isValid() && m_currentStep == CurrentSteps::Downloading) {
        m_fileManager->pauseDownload(m_currentTask.task_id);
    }

    qDebug() << "TaskExecutor stopped";
}

bool TaskExecutor::executeTask(const QString& taskId)
{
    if (!m_initialized) {
        qWarning() << "TaskExecutor not initialized";
        return false;
    }

    // 浠庢湰鍦版暟鎹簱鑾峰彇浠诲姟
    auto task = m_dao->getTransferringTaskById(taskId);
    if (task.task_id.isEmpty()) {
        qWarning() << "Task not found in local database:" << taskId;
        return false;
    }

    // 妫€鏌ヤ换鍔＄姸鎬?
    if (task.status == TransferStatus::Succeeded) {
        qWarning() << "Task already completed:" << taskId;
        return false;
    }

    if (task.status == TransferStatus::Downloading ||
        task.status == TransferStatus::Uploading ||
        task.status == TransferStatus::Installing) {
        qWarning() << "Task already in progress:" << taskId;
        return false;
    }

    // 閲嶇疆浠诲姟鐘舵€侊紙鏀寔閲嶈瘯锛?
    task.status = TransferStatus::Pending;
    task.current_step = CurrentSteps::Idle;
    task.error_message.clear();
    task.transferred_bytes = 0;
    task.last_update_time = QDateTime::currentDateTime();

    if (!m_dao->update(task)) {
        qCritical() << "Failed to reset task status:" << taskId;
        return false;
    }

    // 鍔犲叆闃熷垪
    m_taskQueue.enqueue(task);

    qDebug() << "Task queued for execution:" << taskId;
    emit queueStatusChanged(m_taskQueue.size(), m_currentTask.isValid());

    return true;
}

bool TaskExecutor::cancelTask(const QString& taskId)
{
    // 濡傛灉鏄綋鍓嶆鍦ㄦ墽琛岀殑浠诲姟
    if (m_currentTask.isValid() && m_currentTask.task_id == taskId) {
        // 鍙栨秷姝ｅ湪杩涜鐨勪笅杞?
        if (m_currentStep == CurrentSteps::Downloading) {
            m_fileManager->cancelDownload(taskId);
        }

        // 鏍囪浠诲姟涓哄凡鍙栨秷
        updateLocalTaskStatus(taskId, TransferStatus::Cancelled, CurrentSteps::Cancelled, "用户取消");

        // 娓呯悊褰撳墠浠诲姟
        m_currentTask = TransferringTask();
        m_currentStep = CurrentSteps::Idle;
        m_retryCount = 0;

        emit taskFinished(taskId, false, "任务已取消");
    } else {
        // 浠庨槦鍒椾腑绉婚櫎
        for (int i = 0; i < m_taskQueue.size(); ++i) {
            if (m_taskQueue[i].task_id == taskId) {
                m_taskQueue.removeAt(i);
                updateLocalTaskStatus(taskId, TransferStatus::Cancelled, CurrentSteps::Cancelled, "用户取消");
                break;
            }
        }
    }

    emit queueStatusChanged(m_taskQueue.size(), m_currentTask.isValid());
    return true;
}

void TaskExecutor::pauseCurrentTask()
{
    if (m_currentStep == CurrentSteps::Downloading) {
        m_fileManager->pauseDownload(m_currentTask.task_id);
        updateLocalTaskStatus(m_currentTask.task_id, TransferStatus::Paused, CurrentSteps::Paused);
        qDebug() << "Task paused:" << m_currentTask.task_id;
    }
}

void TaskExecutor::resumeCurrentTask()
{
    if (m_currentStep == CurrentSteps::Downloading) {
        m_fileManager->resumeDownload(m_currentTask.task_id);
        updateLocalTaskStatus(m_currentTask.task_id, TransferStatus::Downloading, CurrentSteps::Downloading);
        qDebug() << "Task resumed:" << m_currentTask.task_id;
    }
}

TransferringTask TaskExecutor::getCurrentTask() const
{
    return m_currentTask;
}

int TaskExecutor::getPendingCount() const
{
    return m_taskQueue.size();
}

bool TaskExecutor::isBusy() const
{
    return m_currentTask.isValid();
}

// ==================== 绉佹湁妲藉嚱鏁?====================

void TaskExecutor::onDownloadProgress(QString taskId, qint64 transferred, qint64 total, int progressPercent)
{
    if (!m_currentTask.isValid() || m_currentTask.task_id != taskId) {
        return;
    }

    // 鏇存柊鏈湴鏁版嵁搴?
    updateLocalTaskProgress(taskId, transferred, progressPercent);

    // 璁＄畻閫熷害骞跺彂閫佷俊鍙?
    calculateSpeed(transferred);
    emit taskProgressUpdated(taskId, "下载中", progressPercent, m_currentSpeed);
}

void TaskExecutor::onDownloadFinished(QString taskId, const QString& localPath, bool success)
{
    if (!m_currentTask.isValid() || m_currentTask.task_id != taskId) {
        return;
    }

    if (success) {
        qDebug() << "Download completed for task:" << taskId << ", file:" << localPath;
        updateLocalTaskStatus(taskId, TransferStatus::Downloading, CurrentSteps::Sending);

        // 鍋滄閫熷害璁＄畻
        m_speedUpdateTimer->stop();

        // 寮€濮嬪彂閫佸埌璁惧
        startSendToDevice(taskId, localPath);
    } else {
        qDebug() << "Download verification failed for task:" << taskId;
        onDownloadFailed(taskId, 1002, "文件 SHA-256 校验失败");
    }
}

void TaskExecutor::onDownloadFailed(QString taskId, int errorCode, const QString& errorMessage)
{
    if (!m_currentTask.isValid() || m_currentTask.task_id != taskId) {
        return;
    }

    qWarning() << "Download failed for task:" << taskId
               << ", error:" << errorCode << "," << errorMessage;

    // 鍋滄閫熷害璁＄畻
    m_speedUpdateTimer->stop();

    // 閲嶈瘯閫昏緫
    if (m_retryCount < MAX_RETRY_COUNT) {
        m_retryCount++;
        qDebug() << "Retrying download, attempt" << m_retryCount << "of" << MAX_RETRY_COUNT;

        QTimer::singleShot(RETRY_DELAY_MS, this, [this]() {
            if (m_currentTask.isValid()) {
                startDownloadTask(m_currentTask);
            }
        });
        return;
    }

    // 閲嶈瘯澶辫触锛屾爣璁颁换鍔″け璐?
    QString fullError = QString("下载失败(重试%1次): %2").arg(MAX_RETRY_COUNT).arg(errorMessage);
    markTaskComplete(taskId, false, fullError);

    // 娓呯悊骞舵墽琛屼笅涓€涓换鍔?
    m_currentTask = TransferringTask();
    m_currentStep = CurrentSteps::Idle;
    m_retryCount = 0;

    processNextTask();
}

void TaskExecutor::onDeviceSendFinished(QString taskId, bool success, const QString& message)
{
    if (!m_currentTask.isValid() || m_currentTask.task_id != taskId) {
        return;
    }

    if (!success) {
        qWarning() << "Send to device failed:" << message;
        markTaskComplete(taskId, false, QString("发送到设备失败: %1").arg(message));

        m_currentTask = TransferringTask();
        m_currentStep = CurrentSteps::Idle;
        processNextTask();
        return;
    }

    qDebug() << "File sent to device, waiting for installation";
    m_currentStep = CurrentSteps::Installing;
    updateLocalTaskStatus(taskId, TransferStatus::Installing, CurrentSteps::Installing);
    emit taskProgressUpdated(taskId, "安装中", 90);
}

void TaskExecutor::onDeviceInstallResult(QString taskId, bool success, const QString& message)
{
    if (!m_currentTask.isValid() || m_currentTask.task_id != taskId) {
        return;
    }

    if (success) {
        qDebug() << "Task completed successfully:" << taskId;
        markTaskComplete(taskId, true, message);
    } else {
        qWarning() << "Installation failed:" << message;
        markTaskComplete(taskId, false, QString("设备安装失败: %1").arg(message));
    }

    // 娓呯悊骞舵墽琛屼笅涓€涓换鍔?
    m_currentTask = TransferringTask();
    m_currentStep = CurrentSteps::Idle;
    m_retryCount = 0;

    processNextTask();
}

void TaskExecutor::onProcessNextTask()
{
    if (!m_isRunning) {
        return;
    }

    // 濡傛灉褰撳墠鏈変换鍔″湪鎵ц锛屼笉鍋氫换浣曚簨
    if (m_currentTask.isValid()) {
        return;
    }

    processNextTask();
}

void TaskExecutor::onUpdateDownloadSpeed()
{
    // 閫熷害宸茬粡鍦?calculateSpeed 涓绠楋紝杩欓噷鍙渶瑕佸彂灏勪俊鍙?
    if (m_currentTask.isValid() && m_currentStep == CurrentSteps::Downloading) {
        auto task = m_dao->getTransferringTaskById(m_currentTask.task_id);
        if (task.isValid()) {
            emit taskProgressUpdated(m_currentTask.task_id, "下载中",
                                     task.getProgressPercent(), m_currentSpeed);
        }
    }
}

// ==================== 绉佹湁鏂规硶 ====================

void TaskExecutor::processNextTask()
{
    if (m_taskQueue.isEmpty()) {
        return;
    }

    // 鍙栧嚭涓嬩竴涓换鍔?
    TransferringTask nextTask = m_taskQueue.dequeue();
    m_currentTask = nextTask;
    m_currentStep = CurrentSteps::Downloading;
    m_retryCount = 0;

    qDebug() << "Starting task:" << nextTask.task_id << "-" << nextTask.description;
    emit taskStarted(nextTask.task_id, nextTask.description);

    // 寮€濮嬩笅杞?
    startDownloadTask(nextTask);
}

void TaskExecutor::startDownloadTask(const TransferringTask& task)
{
    // 鏇存柊鐘舵€佷负涓嬭浇涓?
    updateLocalTaskStatus(task.task_id, TransferStatus::Downloading, CurrentSteps::Downloading);
    emit taskProgressUpdated(task.task_id, "下载中", 0);

    // 閲嶇疆閫熷害缁熻
    m_lastTransferredBytes = task.transferred_bytes;
    m_speedTimer.start();
    m_speedUpdateTimer->start(SPEED_UPDATE_INTERVAL_MS);

    // 鍚姩涓嬭浇
    if (!m_fileManager->startDownload(task)) {
        qCritical() << "Failed to start download for task:" << task.task_id;
        onDownloadFailed(task.task_id, 1000, "无法启动下载");
    }
}

void TaskExecutor::startSendToDevice(const QString& taskId, const QString& localPath)
{
    m_currentStep = CurrentSteps::Sending;
    updateLocalTaskStatus(taskId, TransferStatus::Uploading, CurrentSteps::Sending);
    emit taskProgressUpdated(taskId, "发送中", 70);

    // 鑾峰彇浠诲姟璇︽儏
    auto task = m_dao->getTransferringTaskById(taskId);
    if (!task.isValid()) {
        qCritical() << "Task not found:" << taskId;
        onDeviceSendFinished(taskId, false, "任务不存在");
        return;
    }

    // 妫€鏌ヨ澶囨槸鍚﹀湪绾?
    if (!isDeviceOnline(task.target_device_id)) {
        qWarning() << "Device offline:" << task.target_device_id;
        onDeviceSendFinished(taskId, false, "设备离线，请检查设备连接");
        return;
    }

    // 鍙戦€佹枃浠跺埌璁惧
    m_deviceConnector->sendFileToDevice(taskId, task.target_device_id, localPath, task.file_name, task.file_sha256);
}

void TaskExecutor::updateLocalTaskStatus(const QString& taskId,
                                         TransferStatus::Status status,
                                         CurrentSteps::Steps steps,
                                         const QString& errorMessage)
{
    auto task = m_dao->getTransferringTaskById(taskId);
    if (!task.isValid()) {
        qWarning() << "Task not found for status update:" << taskId;
        return;
    }

    task.status = status;
    task.current_step = steps;
    if (!errorMessage.isEmpty()) {
        task.error_message = errorMessage;
    }
    task.last_update_time = QDateTime::currentDateTime();

    if (!m_dao->update(task)) {
        qCritical() << "Failed to update task status:" << taskId;
    }
}

void TaskExecutor::updateLocalTaskProgress(const QString& taskId,
                                           qint64 transferredBytes,
                                           int progress)
{
    auto task = m_dao->getTransferringTaskById(taskId);
    if (!task.isValid()) {
        return;
    }

    task.transferred_bytes = transferredBytes;
    task.last_update_time = QDateTime::currentDateTime();

    if (!m_dao->update(task)) {
        qCritical() << "Failed to update task progress:" << taskId;
    }
}

void TaskExecutor::markTaskComplete(const QString& taskId, bool success, const QString& message)
{
    auto task = m_dao->getTransferringTaskById(taskId);
    if (!task.isValid()) {
        qCritical() << "Task not found for completion:" << taskId;
        return;
    }

    if (success) {
        task.status = TransferStatus::Succeeded;
        task.current_step = CurrentSteps::Completed;
        task.error_message.clear();
    } else {
        task.status = TransferStatus::Failed;
        task.current_step = CurrentSteps::Failed;
        task.error_message = message;
    }

    task.end_time = QDateTime::currentDateTime();
    task.last_update_time = QDateTime::currentDateTime();

    if (!m_dao->update(task)) {
        qCritical() << "Failed to mark task complete:" << taskId;
    }

    emit taskFinished(taskId, success, message);
}

void TaskExecutor::loadResumableTasks()
{
    // 鍔犺浇鎵€鏈夋湭瀹屾垚鐨勪换鍔★紙绋嬪簭寮傚父閫€鍑烘椂鐣欎笅鐨勶級
    auto downloadingTasks = m_dao->getByStatus(TransferStatus::Downloading);
    auto sendingTasks = m_dao->getByStatus(TransferStatus::Uploading);
    auto installingTasks = m_dao->getByStatus(TransferStatus::Installing);
    auto pendingTasks = m_dao->getByStatus(TransferStatus::Pending);

    // 灏嗘湭瀹屾垚鐨勪换鍔￠噸缃负 Pending 鐘舵€侊紝閲嶆柊鍔犲叆闃熷垪
    auto resetAndEnqueue = [this](const QList<TransferringTask>& tasks) {
        for (auto task : tasks) {
            if (task.status == TransferStatus::Downloading && task.transferred_bytes > 0) {
                // 鏈夐儴鍒嗕笅杞界殑浠诲姟锛屼繚鐣欏凡涓嬭浇瀛楄妭鏁帮紙鏂偣缁紶锛?
                qDebug() << "Resuming partial download:" << task.task_id
                         << "transferred:" << task.transferred_bytes;
            } else if (task.status == TransferStatus::Uploading ||
                       task.status == TransferStatus::Installing) {
                // 鍙戦€?瀹夎涓殑浠诲姟锛岄噸缃负寰呮墽琛岋紙杩欎簺鎿嶄綔涓嶅彲鎭㈠锛?
                task.transferred_bytes = 0;
            }

            task.status = TransferStatus::Pending;
            task.current_step = CurrentSteps::Idle;
            task.error_message.clear();
            task.last_update_time = QDateTime::currentDateTime();

            if (m_dao->update(task)) {
                m_taskQueue.enqueue(task);
            }
        }
    };

    resetAndEnqueue(downloadingTasks);
    resetAndEnqueue(sendingTasks);
    resetAndEnqueue(installingTasks);
    resetAndEnqueue(pendingTasks);

    qDebug() << "Loaded resumable tasks:" << m_taskQueue.size();
}

bool TaskExecutor::isDeviceOnline(const QString& deviceId)
{
    return m_deviceConnector->isDeviceOnline(deviceId);
}

void TaskExecutor::calculateSpeed(qint64 bytesTransferred)
{
    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed >= 1000) {
        qint64 bytesDiff = bytesTransferred - m_lastTransferredBytes;
        m_currentSpeed = bytesDiff * 1000 / elapsed;
        m_lastTransferredBytes = bytesTransferred;
        m_speedTimer.restart();
    }
}

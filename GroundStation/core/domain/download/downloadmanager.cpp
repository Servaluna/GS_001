#include "downloadmanager.h"

#include "../../network/serverconnector.h"
#include "../../repository/taskrepository.h"
#include "../../logging/logger.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

constexpr int AUTO_SAVE_INTERVAL_MS = 5000;
constexpr int PROGRESS_SAVE_THRESHOLD = 5;

DownloadManager::DownloadContext::DownloadContext(const DeviceTask& deviceTask,
                                                  const DownloadTask& downloadTask)
    : taskUuid(downloadTask.task_uuid)
    , deviceTaskId(deviceTask.device_task_id)
    , fileCode(downloadTask.file_code.isEmpty() ? deviceTask.file_code : downloadTask.file_code)
    , localTempPath(downloadTask.temp_path.isEmpty() ? downloadTask.local_path + ".part" : downloadTask.temp_path)
    , localPath(downloadTask.local_path)
    , totalSize(deviceTask.total_size)
    , downloadedSize(downloadTask.downloaded_size)
    , expectedSha256(downloadTask.checksum_sha256)
    , isPaused(false)
    , isCancelled(false)
    , tempFile(nullptr)
    , lastSavedProgress(-1)
{}

DownloadManager::DownloadContext::~DownloadContext()
{
    if (tempFile) {
        tempFile->close();
        delete tempFile;
        tempFile = nullptr;
    }
}

DownloadManager::DownloadManager(QObject *parent)
    : QObject{parent}
    , m_serverConnector(nullptr)
    , m_repository(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_initialized(false)
{}

DownloadManager::~DownloadManager()
{
    for (auto context : m_downloads.values()) {
        delete context;
    }
    m_downloads.clear();
}

bool DownloadManager::init(ServerConnector* serverConnector, TaskRepository* repository)
{
    if (!serverConnector || !repository) {
        Logger::error("DOWNLOAD_MANAGER_INIT_FAILED", "DownloadManager 初始化失败，依赖为空");
        return false;
    }

    m_serverConnector = serverConnector;
    m_repository = repository;

    connect(m_serverConnector, &ServerConnector::fileChunkReceived,
            this, &DownloadManager::onFileChunkReceived);
    connect(m_serverConnector, &ServerConnector::fileInfoReceived,
            this, &DownloadManager::onFileInfoReceived);
    connect(m_serverConnector, &ServerConnector::errorOccurred,
            this, [this](const QString& msg) {
                for (const QString& taskUuid : m_downloads.keys()) {
                    onServerError(taskUuid, 0, msg);
                }
            });

    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &DownloadManager::onAutoSaveProgress);
    m_autoSaveTimer->start(AUTO_SAVE_INTERVAL_MS);

    m_initialized = true;
    Logger::info("DOWNLOAD_MANAGER_READY", "下载管理器初始化完成");
    return true;
}

bool DownloadManager::startDownload(const DeviceTask& deviceTask, const DownloadTask& downloadTask)
{
    if (!m_initialized) {
        Logger::warn("DOWNLOAD_START_FAILED", "下载管理器尚未初始化");
        return false;
    }

    if (!deviceTask.isValid() || !downloadTask.isValid()) {
        Logger::warn("DOWNLOAD_START_FAILED", "下载任务无效");
        return false;
    }

    if (m_downloads.contains(downloadTask.task_uuid)) {
        Logger::warn("DOWNLOAD_START_REJECTED",
                     "下载任务已经在执行",
                     {{"download_session_id", downloadTask.task_uuid}, {"device_task_id", deviceTask.device_task_id}});
        return false;
    }

    if (isLocalFileValid(downloadTask, deviceTask.total_size)) {
        Logger::info("DOWNLOAD_SKIPPED",
                     "本地文件已存在且校验通过，跳过下载",
                     {{"download_session_id", downloadTask.task_uuid}, {"device_task_id", deviceTask.device_task_id}, {"local_path", downloadTask.local_path}});
        emit downloadFinished(downloadTask.task_uuid, downloadTask.local_path, true);
        return true;
    }

    DownloadContext* context = new DownloadContext(deviceTask, downloadTask);
    QDir().mkpath(QFileInfo(context->localPath).absolutePath());
    QDir().mkpath(QFileInfo(context->localTempPath).absolutePath());

    context->tempFile = new QFile(context->localTempPath);
    if (context->downloadedSize > 0) {
        if (!context->tempFile->open(QIODevice::ReadWrite)) {
            Logger::error("DOWNLOAD_START_FAILED",
                          "无法打开断点续传临时文件",
                          {{"download_session_id", context->taskUuid}, {"temp_path", context->localTempPath}});
            delete context;
            return false;
        }

        const qint64 actualSize = context->tempFile->size();
        if (actualSize != context->downloadedSize) {
            context->downloadedSize = 0;
            context->tempFile->resize(0);
        } else {
            context->tempFile->seek(context->downloadedSize);
        }
    } else if (!context->tempFile->open(QIODevice::WriteOnly)) {
        Logger::error("DOWNLOAD_START_FAILED",
                      "无法创建临时文件",
                      {{"download_session_id", context->taskUuid}, {"temp_path", context->localTempPath}});
        delete context;
        return false;
    }

    m_downloads.insert(context->taskUuid, context);

    if (!fileDownloadRequest(context->taskUuid, context->downloadedSize)) {
        cleanupContext(context->taskUuid);
        return false;
    }

    Logger::info("DOWNLOAD_START",
                 QString("开始下载设备任务 %1 的文件").arg(deviceTask.device_task_id),
                 {{"download_session_id", context->taskUuid}, {"device_task_id", deviceTask.device_task_id}, {"file_code", context->fileCode}, {"offset", context->downloadedSize}});
    return true;
}

bool DownloadManager::pauseDownload(const QString& taskUuid)
{
    DownloadContext* context = getContext(taskUuid);
    if (!context) {
        return false;
    }

    context->isPaused = true;
    saveProgressToDatabase(*context, DownloadSessionStatus::Paused);
    Logger::info("DOWNLOAD_PAUSED", "下载已暂停", {{"download_session_id", taskUuid}});
    emit downloadPaused(taskUuid);
    return true;
}

bool DownloadManager::resumeDownload(const QString& taskUuid)
{
    DownloadContext* context = getContext(taskUuid);
    if (!context || !context->isPaused) {
        return false;
    }

    context->isPaused = false;
    if (!fileDownloadRequest(taskUuid, context->downloadedSize)) {
        return false;
    }

    Logger::info("DOWNLOAD_RESUMED", "下载已恢复", {{"download_session_id", taskUuid}, {"offset", context->downloadedSize}});
    emit downloadResumed(taskUuid);
    return true;
}

bool DownloadManager::cancelDownload(const QString& taskUuid)
{
    DownloadContext* context = getContext(taskUuid);
    if (!context) {
        return false;
    }

    context->isCancelled = true;
    Logger::warn("DOWNLOAD_CANCELLED", "下载已取消", {{"download_session_id", taskUuid}});
    cleanupContext(taskUuid, true);
    return true;
}

int DownloadManager::getProgress(const QString& taskUuid) const
{
    const DownloadContext* context = m_downloads.value(taskUuid);
    if (context && context->totalSize > 0) {
        return static_cast<int>((context->downloadedSize * 100) / context->totalSize);
    }

    const DownloadTask task = m_repository->getDownloadTask(taskUuid);
    return task.isValid() && task.status == DownloadSessionStatus::Finished ? 100 : 0;
}

bool DownloadManager::isLocalFileValid(const DownloadTask& downloadTask, qint64 expectedSize) const
{
    QFileInfo fileInfo(downloadTask.local_path);
    if (!fileInfo.exists()) {
        return false;
    }

    if (expectedSize > 0 && fileInfo.size() != expectedSize) {
        return false;
    }

    if (downloadTask.checksum_sha256.isEmpty()) {
        return true;
    }

    const QString actualSha256 = calculateFileSha256(downloadTask.local_path);
    return actualSha256.compare(downloadTask.checksum_sha256, Qt::CaseInsensitive) == 0;
}

void DownloadManager::onFileChunkReceived(QString taskUuid, const QByteArray& chunkData, int chunkIndex, bool isLast)
{
    Q_UNUSED(chunkIndex);

    DownloadContext* context = getContext(taskUuid);
    if (!context || context->isPaused || context->isCancelled) {
        return;
    }

    const qint64 bytesWritten = context->tempFile->write(chunkData);
    if (bytesWritten != chunkData.size()) {
        Logger::error("DOWNLOAD_FAILED", "写入临时文件失败", {{"download_session_id", taskUuid}});
        emit downloadFailed(taskUuid, 1001, "写入临时文件失败");
        cleanupContext(taskUuid);
        return;
    }

    context->tempFile->flush();
    context->downloadedSize += bytesWritten;

    int progressPercent = 0;
    if (context->totalSize > 0) {
        progressPercent = static_cast<int>((context->downloadedSize * 100) / context->totalSize);
    }

    emit progressUpdated(taskUuid, context->downloadedSize, context->totalSize, progressPercent);

    if (qAbs(progressPercent - context->lastSavedProgress) >= PROGRESS_SAVE_THRESHOLD) {
        saveProgressToDatabase(*context, DownloadSessionStatus::Downloading);
        context->lastSavedProgress = progressPercent;
    }

    if (isLast || (context->totalSize > 0 && context->downloadedSize >= context->totalSize)) {
        const bool success = finalizeDownload(*context);
        if (success) {
            emit downloadFinished(taskUuid, context->localPath, true);
        } else {
            emit downloadFailed(taskUuid, 1002, "文件 SHA-256 校验失败");
        }
        cleanupContext(taskUuid, !success);
    }
}

void DownloadManager::onFileInfoReceived(QString taskUuid, qint64 totalSize, const QString& sha256)
{
    DownloadContext* context = getContext(taskUuid);
    if (!context) {
        return;
    }

    if (context->totalSize > 0 && totalSize != context->totalSize) {
        Logger::error("DOWNLOAD_FAILED",
                      "文件大小不匹配",
                      {{"download_session_id", taskUuid}, {"expected_size", context->totalSize}, {"actual_size", totalSize}});
        emit downloadFailed(taskUuid, 1003, "文件大小不匹配");
        cleanupContext(taskUuid);
        return;
    }

    if (!context->expectedSha256.isEmpty() &&
        sha256.compare(context->expectedSha256, Qt::CaseInsensitive) != 0) {
        Logger::error("DOWNLOAD_FAILED",
                      "服务器返回的 SHA-256 不匹配",
                      {{"download_session_id", taskUuid}, {"expected_sha256", context->expectedSha256}, {"actual_sha256", sha256}});
        emit downloadFailed(taskUuid, 1004, "服务器返回的 SHA-256 不匹配");
        cleanupContext(taskUuid);
        return;
    }

    context->totalSize = totalSize;
    context->expectedSha256 = sha256;
}

void DownloadManager::onServerError(QString taskUuid, int errorCode, const QString& errorMessage)
{
    if (!getContext(taskUuid)) {
        return;
    }

    Logger::error("DOWNLOAD_FAILED",
                  "服务器下载错误",
                  {{"download_session_id", taskUuid}, {"error_code", errorCode}, {"error_message", errorMessage}});
    emit downloadFailed(taskUuid, errorCode, errorMessage);
    cleanupContext(taskUuid);
}

void DownloadManager::onAutoSaveProgress()
{
    for (const DownloadContext* context : m_downloads.values()) {
        if (context && !context->isPaused && !context->isCancelled) {
            saveProgressToDatabase(*context, DownloadSessionStatus::Downloading);
        }
    }
}

bool DownloadManager::fileDownloadRequest(const QString& taskUuid, qint64 offset)
{
    if (!m_serverConnector) {
        Logger::error("DOWNLOAD_REQUEST_FAILED", "ServerConnector 为空");
        return false;
    }

    DownloadContext* context = getContext(taskUuid);
    if (!context) {
        return false;
    }

    return m_serverConnector->fileDownloadRequest(context->fileCode, offset, taskUuid);
}

QString DownloadManager::calculateFileSha256(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }

    return QString(hash.result().toHex());
}

void DownloadManager::saveProgressToDatabase(const DownloadContext& context, DownloadSessionStatus status)
{
    if (!m_repository) {
        return;
    }

    m_repository->updateDownloadProgress(context.taskUuid,
                                         context.downloadedSize,
                                         context.expectedSha256,
                                         status);

    const double deviceProgress = context.totalSize > 0
        ? (context.downloadedSize * 70.0 / context.totalSize)
        : 0.0;
    m_repository->updateDeviceProgress(context.deviceTaskId, context.downloadedSize, deviceProgress);
}

bool DownloadManager::finalizeDownload(DownloadContext& context)
{
    if (context.tempFile) {
        context.tempFile->close();
    }

    if (!context.expectedSha256.isEmpty()) {
        const QString actualSha256 = calculateFileSha256(context.localTempPath);
        if (actualSha256.compare(context.expectedSha256, Qt::CaseInsensitive) != 0) {
            Logger::error("DOWNLOAD_VERIFY_FAILED",
                          "SHA-256 校验失败",
                          {{"download_session_id", context.taskUuid}, {"expected_sha256", context.expectedSha256}, {"actual_sha256", actualSha256}});
            return false;
        }
    }

    QFile tempFile(context.localTempPath);
    if (tempFile.exists()) {
        if (QFile::exists(context.localPath)) {
            QFile::remove(context.localPath);
        }

        if (!tempFile.rename(context.localPath)) {
            Logger::error("DOWNLOAD_FINALIZE_FAILED",
                          "临时文件重命名失败",
                          {{"download_session_id", context.taskUuid}, {"local_path", context.localPath}, {"temp_path", context.localTempPath}});
            return false;
        }
    }

    context.downloadedSize = context.totalSize;
    saveProgressToDatabase(context, DownloadSessionStatus::Finished);
    m_repository->updateDeviceLocalPackagePath(context.deviceTaskId, context.localPath);
    Logger::info("DOWNLOAD_FINISHED",
                 "文件下载完成",
                 {{"download_session_id", context.taskUuid}, {"device_task_id", context.deviceTaskId}, {"local_path", context.localPath}});
    return true;
}

void DownloadManager::cleanupContext(const QString& taskUuid, bool removeFile)
{
    DownloadContext* context = m_downloads.take(taskUuid);
    if (!context) {
        return;
    }

    if (removeFile && context->tempFile) {
        context->tempFile->close();
        QFile::remove(context->localTempPath);
    }

    delete context;
}

DownloadManager::DownloadContext* DownloadManager::getContext(const QString& taskUuid)
{
    return m_downloads.value(taskUuid, nullptr);
}

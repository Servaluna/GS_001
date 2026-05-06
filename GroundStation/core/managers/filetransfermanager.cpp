#include "filetransfermanager.h"
#include "../network/serverconnector.h"
#include "../localdatabase/localdao.h"

// #include "../Common/protocol.h"

#include <QDir>
#include <QCryptographicHash>

// 甯搁噺瀹氫箟
constexpr int AUTO_SAVE_INTERVAL_MS = 5000;  // 姣?绉掕嚜鍔ㄤ繚瀛樹竴娆¤繘搴?
constexpr int PROGRESS_SAVE_THRESHOLD = 5;   // 杩涘害鍙樺寲瓒呰繃5%鎵嶄繚瀛樺埌鏁版嵁搴?

FileTransferManager::FileTransferManager(QObject *parent)
    : QObject{parent}
    , m_serverConnector(nullptr)
    , m_dao(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_initialized(false)
{}

FileTransferManager::~FileTransferManager()
{
    // 娓呯悊鎵€鏈変笅杞戒笂涓嬫枃
    for (auto context : m_downloads.values()) {
        if (context) {
            delete context;
        }
    }
    m_downloads.clear();

    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        delete m_autoSaveTimer;
    }
}

bool FileTransferManager::init(ServerConnector* serverConnector, LocalDAO* dao)
{
    if (!serverConnector || !dao) {
        qCritical() << "FileTransferManager::init - Invalid parameters";
        return false;
    }

    m_serverConnector = serverConnector;
    m_dao = dao;

    // 杩炴帴鏈嶅姟鍣ㄤ俊鍙?
    connect(m_serverConnector, &ServerConnector::fileChunkReceived,
            this, &FileTransferManager::onFileChunkReceived);
    connect(m_serverConnector, &ServerConnector::fileInfoReceived,
            this, &FileTransferManager::onFileInfoReceived);
    connect(m_serverConnector, &ServerConnector::errorOccurred,
            this, [this](const QString& msg) {
                for (const auto& taskId : m_downloads.keys()) {
                    onServerError(taskId, 0, msg);
                }
            });

    // 鍒涘缓鑷姩淇濆瓨瀹氭椂鍣?
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &FileTransferManager::onAutoSaveProgress);
    m_autoSaveTimer->start(AUTO_SAVE_INTERVAL_MS);

    m_initialized = true;
    qDebug() << "FileTransferManager initialized";
    return true;
}

bool FileTransferManager::startDownload(const TransferringTask& task)
{
    if (!m_initialized) {
        qWarning() << "FileTransferManager not initialized";
        return false;
    }

    // 妫€鏌ユ槸鍚﹀凡鍦ㄤ笅杞戒腑
    if (m_downloads.contains(task.task_id)) {
        qWarning() << "Task" << task.task_id << "is already downloading";
        return false;
    }

    // 妫€鏌ユ湰鍦版槸鍚﹀凡鏈夊畬鏁翠笖鏈夋晥鐨勬枃浠?
    if (isLocalFileValid(task.task_id, task.file_sha256)) {
        qDebug() << "Task" << task.task_id << "file already exists and valid, skip download";
        emit downloadFinished(task.task_id, task.local_cache_path, true);
        return true;
    }

    // 鍒涘缓涓嬭浇涓婁笅鏂?
    DownloadContext* context = new DownloadContext(task);

    // 璁剧疆鏂囦欢璺緞
    // QString cacheDir = "cache/files/";
    // QDir().mkpath(cacheDir);
    // context->localTempPath = cacheDir + QString("temp_%1_%2").arg(task.task_id).arg(task.file_name);
    // context->localCachePath = cacheDir + task.file_name;
    QDir().mkpath(QFileInfo(task.local_cache_path).path());
    QDir().mkpath(QFileInfo(task.local_temp_path).path());


    // 鎵撳紑涓存椂鏂囦欢锛堣拷鍔犳ā寮忥紝鏀寔鏂偣缁紶锛?
    context->tempFile = new QFile(context->localTempPath);
    if (context->transferredBytes > 0) {
        // 鏂偣缁紶锛氭墦寮€鐜版湁鏂囦欢
        if (!context->tempFile->open(QIODevice::ReadWrite)) {
            qCritical() << "Failed to open temp file for resume:" << context->localTempPath;
            delete context;
            return false;
        }

        // 楠岃瘉鐜版湁鏂囦欢澶у皬鏄惁鍖归厤
        qint64 actualSize = context->tempFile->size();
        if (actualSize != context->transferredBytes) {
            qWarning() << "File size mismatch, resetting download";
            context->transferredBytes = 0;
            context->tempFile->resize(0);
        } else {
            context->tempFile->seek(context->transferredBytes);
        }
    } else {
        // 鏂颁笅杞斤細鍒涘缓鏂版枃浠?
        if (!context->tempFile->open(QIODevice::WriteOnly)) {
            qCritical() << "Failed to create temp file:" << context->localTempPath;
            delete context;
            return false;
        }
    }

    m_downloads[task.task_id] = context;

    // 鍙戦€佷笅杞借姹傚埌鏈嶅姟鍣?
    if (!fileDownloadRequest(task.task_id, context->transferredBytes)) {
        cleanupContext(task.task_id);
        return false;
    }

    qDebug() << "Started download for task" << task.task_id
             << "resume from" << context->transferredBytes << "bytes";
    return true;
}

bool FileTransferManager::pauseDownload(QString taskId)
{
    DownloadContext* context = getContext(taskId);
    if (!context) {
        qWarning() << "Task" << taskId << "not found";
        return false;
    }

    context->isPaused = true;
    saveProgressToDatabase(*context);
    emit downloadPaused(taskId);

    qDebug() << "Paused download for task" << taskId;
    return true;
}

bool FileTransferManager::resumeDownload(QString taskId)
{
    DownloadContext* context = getContext(taskId);
    if (!context) {
        qWarning() << "Task" << taskId << "not found";
        return false;
    }

    if (!context->isPaused) {
        qWarning() << "Task" << taskId << "is not paused";
        return false;
    }

    context->isPaused = false;

    // 閲嶆柊璇锋眰涓嬭浇
    if (!fileDownloadRequest(taskId, context->transferredBytes)) {
        return false;
    }

    emit downloadResumed(taskId);
    qDebug() << "Resumed download for task" << taskId;
    return true;
}

bool FileTransferManager::cancelDownload(QString taskId)
{
    DownloadContext* context = getContext(taskId);
    if (!context) {
        qWarning() << "Task" << taskId << "not found";
        return false;
    }

    context->isCancelled = true;
    cleanupContext(taskId, true);  // 鍒犻櫎涓存椂鏂囦欢
    qDebug() << "Cancelled download for task" << taskId;
    return true;
}

int FileTransferManager::getProgress(QString taskId) const
{
    const DownloadContext* context = m_downloads.value(taskId);
    if (context && context->totalSize > 0) {
        return static_cast<int>((context->transferredBytes * 100) / context->totalSize);
    }

    // 濡傛灉涓嶅湪娲诲姩涓嬭浇涓紝浠庢暟鎹簱鏌ヨ
    auto task = m_dao->getTransferringTaskById(taskId);
    if (!task.task_id.isEmpty() && task.file_size > 0) {
        return static_cast<int>((task.transferred_bytes * 100) / task.file_size);
    }

    return 0;
}

bool FileTransferManager::isLocalFileValid(QString taskId, const QString& expectedSha256) const
{
    // 浠庢暟鎹簱鑾峰彇浠诲姟淇℃伅
    auto task = m_dao->getTransferringTaskById(taskId);
    if (task.task_id.isEmpty()) {
        return false;
    }

    // QString cacheDir = "cache/files/";
    QString localPath = task.local_cache_path;

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || fileInfo.size() != task.file_size) {
        return false;
    }

    QString actualSha256 = const_cast<FileTransferManager*>(this)->calculateFileSha256(localPath);
    return actualSha256.compare(expectedSha256, Qt::CaseInsensitive) == 0;
}

void FileTransferManager::onFileChunkReceived(QString taskId, const QByteArray& chunkData, int chunkIndex, bool isLast)
{
    Q_UNUSED(chunkIndex);  // 鍛婅瘔缂栬瘧鍣ㄨ繖涓弬鏁版槸鏈夋剰鏈娇鐢ㄧ殑

    DownloadContext* context = getContext(taskId);
    if (!context || context->isPaused || context->isCancelled) {
        return;
    }

    // 鍐欏叆鏁版嵁
    qint64 bytesWritten = context->tempFile->write(chunkData);
    if (bytesWritten != chunkData.size()) {
        emit downloadFailed(taskId, 1001, "Failed to write to temp file");
        cleanupContext(taskId);
        return;
    }

    context->tempFile->flush();
    context->transferredBytes += bytesWritten;

    // 璁＄畻杩涘害
    int progressPercent = 0;
    if (context->totalSize > 0) {
        progressPercent = static_cast<int>((context->transferredBytes * 100) / context->totalSize);
    }

    // 鍙戦€佽繘搴︿俊鍙?
    emit progressUpdated(taskId, context->transferredBytes, context->totalSize, progressPercent);

    // 鑷姩淇濆瓨杩涘害锛堜粎褰撹繘搴﹀彉鍖栬秴杩囬槇鍊兼椂锛?
    int progressChange = qAbs(progressPercent - context->lastSavedProgress);
    if (progressChange >= PROGRESS_SAVE_THRESHOLD) {
        saveProgressToDatabase(*context);
        context->lastSavedProgress = progressPercent;
    }

    // 妫€鏌ユ槸鍚﹀畬鎴?
    if (isLast || context->transferredBytes >= context->totalSize) {
        qDebug() << "Download completed for task" << taskId;
        bool success = finalizeDownload(*context);
        if (success) {
            emit downloadFinished(taskId, context->localCachePath, true);
        } else {
            emit downloadFailed(taskId, 1002, "SHA-256 verification failed");
        }
        cleanupContext(taskId, !success);  // 澶辫触鏃跺垹闄や复鏃舵枃浠?
    }
}

void FileTransferManager::onFileInfoReceived(QString taskId, qint64 totalSize, const QString& sha256)
{
    DownloadContext* context = getContext(taskId);
    if (!context) {
        return;
    }

    // 楠岃瘉鏂囦欢淇℃伅
    if (totalSize != context->totalSize) {
        qWarning() << "File size mismatch:" << totalSize << "vs" << context->totalSize;
        emit downloadFailed(taskId, 1003, "File size mismatch");
        cleanupContext(taskId);
        return;
    }

    if (sha256.compare(context->expectedSha256, Qt::CaseInsensitive) != 0) {
        qWarning() << "SHA-256 mismatch:" << sha256 << "vs" << context->expectedSha256;
        emit downloadFailed(taskId, 1004, "SHA-256 mismatch from server");
        cleanupContext(taskId);
        return;
    }

    qDebug() << "File info verified for task" << taskId;
}

void FileTransferManager::onServerError(QString taskId, int errorCode, const QString& errorMessage)
{
    DownloadContext* context = getContext(taskId);
    if (!context) {
        return;
    }

    emit downloadFailed(taskId, errorCode, errorMessage);
    cleanupContext(taskId);
}

void FileTransferManager::onAutoSaveProgress()
{
    // 淇濆瓨鎵€鏈夋椿鍔ㄤ笅杞界殑杩涘害
    for (auto context : m_downloads.values()) {
        if (context && !context->isPaused && !context->isCancelled) {
            saveProgressToDatabase(*context);
        }
    }
}

bool FileTransferManager::fileDownloadRequest(QString taskId, qint64 offset)
{
    if (!m_serverConnector) {
        qCritical() << "ServerConnector is null";
        return false;
    }

    // 鑾峰彇浠诲姟淇℃伅浠ヨ幏鍙杅ile_id
    auto task = m_dao->getTransferringTaskById(taskId);
    if (task.task_id.isEmpty()) {
        qCritical() << "Task" << taskId << "not found in database";
        return false;
    }

    // 鍙戦€佽姹傚埌鏈嶅姟鍣?
    // 杩欓噷鍋囪ServerConnector鏈塺equestFileChunk鏂规硶
    // 瀹為檯浣跨敤鏃堕渶瑕佹牴鎹綘鐨凷erverConnector API璋冩暣
    bool result = m_serverConnector->fileDownloadRequest(task.file_id, offset);
    if (!result) {
        qCritical() << "Failed to send download request for task" << taskId;
        return false;
    }

    return true;
}

QString FileTransferManager::calculateFileSha256(const QString& filePath) const
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

void FileTransferManager::saveProgressToDatabase(const DownloadContext& context)
{
    if (!m_dao) {
        return;
    }

    auto task = m_dao->getTransferringTaskById(context.taskId);
    if (task.task_id.isEmpty()) {
        return;
    }

    task.transferred_bytes = context.transferredBytes;
    task.status = TransferStatus::Downloading;
    task.error_message.clear();

    m_dao->update(task);
}

bool FileTransferManager::finalizeDownload(DownloadContext& context)
{
    // 鍏抽棴涓存椂鏂囦欢
    if (context.tempFile) {
        context.tempFile->close();
    }

    QString actualSha256 = calculateFileSha256(context.localTempPath);
    if (actualSha256.compare(context.expectedSha256, Qt::CaseInsensitive) != 0) {
        qCritical() << "SHA-256 verification failed for task" << context.taskId
                    << "Expected:" << context.expectedSha256
                    << "Actual:" << actualSha256;
        return false;
    }

    // 閲嶅懡鍚嶄复鏃舵枃浠朵负鏈€缁堟枃浠?
    QFile tempFile(context.localTempPath);
    if (tempFile.exists()) {
        // 濡傛灉鏈€缁堟枃浠跺凡瀛樺湪锛屽厛鍒犻櫎
        if (QFile::exists(context.localCachePath)) {
            QFile::remove(context.localCachePath);
        }

        if (!tempFile.rename(context.localCachePath)) {
            qCritical() << "Failed to rename temp file to" << context.localCachePath;
            return false;
        }
    }

    // 鏇存柊鏁版嵁搴擄細鏍囪涓嬭浇瀹屾垚
    TransferringTask task = m_dao->getTransferringTaskById(context.taskId);
    if (!task.task_id.isEmpty()) {
        task.status = TransferStatus::Succeeded;
        task.transferred_bytes = context.totalSize;
        task.local_cache_path = context.localCachePath;
        m_dao->update(task);
    }

    qDebug() << "Download finalized for task" << context.taskId
             << "File saved to" << context.localCachePath;
    return true;
}

void FileTransferManager::cleanupContext(QString taskId, bool removeFile)
{
    DownloadContext* context = m_downloads.take(taskId);
    if (!context) {
        return;
    }

    if (removeFile && context->tempFile) {
        context->tempFile->close();
        QFile::remove(context->localTempPath);
    }

    delete context;
}

FileTransferManager::DownloadContext* FileTransferManager::getContext(QString taskId)
{
    return m_downloads.value(taskId, nullptr);
}

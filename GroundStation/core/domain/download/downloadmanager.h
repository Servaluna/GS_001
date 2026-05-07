#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include "../../models/devicetask.h"
#include "../../models/downloadtask.h"

#include <QFile>
#include <QMap>
#include <QObject>
#include <QTimer>

class ServerConnector;
class TaskRepository;

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    bool init(ServerConnector* serverConnector, TaskRepository* repository);
    bool startDownload(const DeviceTask& deviceTask, const DownloadTask& downloadTask);
    bool pauseDownload(const QString& taskUuid);
    bool resumeDownload(const QString& taskUuid);
    bool cancelDownload(const QString& taskUuid);
    int getProgress(const QString& taskUuid) const;
    bool isLocalFileValid(const DownloadTask& downloadTask, qint64 expectedSize) const;

signals:
    void progressUpdated(QString taskUuid, qint64 transferred, qint64 total, int progressPercent);
    void downloadFinished(QString taskUuid, const QString& localPath, bool success);
    void downloadFailed(QString taskUuid, int errorCode, const QString& errorMessage);
    void downloadPaused(QString taskUuid);
    void downloadResumed(QString taskUuid);

private slots:
    void onFileChunkReceived(QString taskUuid, const QByteArray& chunkData, int chunkIndex, bool isLast);
    void onFileInfoReceived(QString taskUuid, qint64 totalSize, const QString& sha256);
    void onServerError(QString taskUuid, int errorCode, const QString& errorMessage);
    void onAutoSaveProgress();

private:
    struct DownloadContext {
        QString taskUuid;
        int deviceTaskId;
        QString fileCode;
        QString localTempPath;
        QString localPath;
        qint64 totalSize;
        qint64 downloadedSize;
        QString expectedSha256;
        bool isPaused;
        bool isCancelled;
        QFile* tempFile;
        int lastSavedProgress;

        DownloadContext(const DeviceTask& deviceTask, const DownloadTask& downloadTask);
        ~DownloadContext();
    };

    bool fileDownloadRequest(const QString& taskUuid, qint64 offset = 0);
    QString calculateFileSha256(const QString& filePath) const;
    void saveProgressToDatabase(const DownloadContext& context, DownloadSessionStatus status);
    bool finalizeDownload(DownloadContext& context);
    void cleanupContext(const QString& taskUuid, bool removeFile = true);
    DownloadContext* getContext(const QString& taskUuid);

    ServerConnector* m_serverConnector;
    TaskRepository* m_repository;
    QMap<QString, DownloadContext*> m_downloads;
    QTimer* m_autoSaveTimer;
    bool m_initialized;
};

#endif // DOWNLOADMANAGER_H

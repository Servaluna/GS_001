#ifndef DOWNLOADCHECKPOINTDAO_H
#define DOWNLOADCHECKPOINTDAO_H

#include "../models/downloadtask.h"

#include <QList>

class DownloadCheckpointDAO
{
public:
    DownloadCheckpointDAO() = default;

    bool upsert(const DownloadTask& task) const;
    bool remove(const QString& taskUuid) const;
    DownloadTask getByTaskUuid(const QString& taskUuid) const;
    QList<DownloadTask> getByOwner(int ownerUserId) const;
    QList<DownloadTask> getByStatus(DownloadSessionStatus status) const;
    bool updateProgress(const QString& taskUuid,
                        qint64 downloadedSize,
                        const QString& checksumSha256,
                        DownloadSessionStatus status) const;
};

#endif // DOWNLOADCHECKPOINTDAO_H

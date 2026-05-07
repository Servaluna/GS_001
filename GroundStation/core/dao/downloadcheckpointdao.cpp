#include "downloadcheckpointdao.h"

#include "../localdatabase/localdatabase.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

DownloadTask downloadTaskFromQuery(const QSqlQuery& query)
{
    return DownloadTask::fromSqlRecord(query.record());
}

QList<DownloadTask> readDownloadTasks(QSqlQuery& query)
{
    QList<DownloadTask> tasks;
    while (query.next()) {
        tasks.append(downloadTaskFromQuery(query));
    }
    return tasks;
}

}

bool DownloadCheckpointDAO::upsert(const DownloadTask& task) const
{
    if (!task.isValid()) {
        qWarning() << "无效下载会话，无法保存";
        return false;
    }

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO download_session (
            download_session_id, device_task_id, file_code, status, progress,
            local_path, temp_path, downloaded_size, total_size, checksum_sha256,
            started_at, updated_at, finished_at, expire_time, error_message
        ) VALUES (
            :download_session_id, :device_task_id, :file_code, :status, :progress,
            :local_path, :temp_path, :downloaded_size, :total_size, :checksum_sha256,
            :started_at, :updated_at, :finished_at, :expire_time, :error_message
        )
    )");
    query.bindValue(":download_session_id", task.task_uuid);
    query.bindValue(":device_task_id", task.device_task_id);
    query.bindValue(":file_code", task.file_code);
    query.bindValue(":status", TaskStatusText::toInt(task.status));
    query.bindValue(":progress", task.total_size > 0 ? task.downloaded_size * 100.0 / task.total_size : 0.0);
    query.bindValue(":local_path", task.local_path);
    query.bindValue(":temp_path", task.temp_path);
    query.bindValue(":downloaded_size", qint64(task.downloaded_size));
    query.bindValue(":total_size", qint64(task.total_size));
    query.bindValue(":checksum_sha256", task.checksum_sha256);
    query.bindValue(":started_at", task.created_at.toString(Qt::ISODate));
    query.bindValue(":updated_at", task.updated_at.toString(Qt::ISODate));
    query.bindValue(":finished_at", task.status == DownloadSessionStatus::Finished ? QDateTime::currentDateTime().toString(Qt::ISODate) : QString());
    query.bindValue(":expire_time", task.expire_time.toString(Qt::ISODate));
    query.bindValue(":error_message", task.error_message);

    if (!query.exec()) {
        qWarning() << "保存下载会话失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DownloadCheckpointDAO::remove(const QString& taskUuid) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("DELETE FROM download_session WHERE download_session_id = :download_session_id");
    query.bindValue(":download_session_id", taskUuid);
    return query.exec();
}

DownloadTask DownloadCheckpointDAO::getByTaskUuid(const QString& taskUuid) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM download_session WHERE download_session_id = :download_session_id");
    query.bindValue(":download_session_id", taskUuid);

    if (query.exec() && query.next()) {
        return downloadTaskFromQuery(query);
    }
    return DownloadTask();
}

QList<DownloadTask> DownloadCheckpointDAO::getByOwner(int ownerUserId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        SELECT ds.*
        FROM download_session ds
        JOIN device_upgrade_task dut ON dut.device_task_id = ds.device_task_id
        WHERE dut.owner_user_id = :owner_user_id
        ORDER BY ds.updated_at DESC
    )");
    query.bindValue(":owner_user_id", ownerUserId);

    if (!query.exec()) {
        qWarning() << "查询用户下载会话失败:" << query.lastError().text();
        return {};
    }
    return readDownloadTasks(query);
}

QList<DownloadTask> DownloadCheckpointDAO::getByStatus(DownloadSessionStatus status) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM download_session WHERE status = :status ORDER BY updated_at DESC");
    query.bindValue(":status", TaskStatusText::toInt(status));

    if (!query.exec()) {
        qWarning() << "按状态查询下载会话失败:" << query.lastError().text();
        return {};
    }
    return readDownloadTasks(query);
}

bool DownloadCheckpointDAO::updateProgress(const QString& taskUuid,
                                           qint64 downloadedSize,
                                           const QString& checksumSha256,
                                           DownloadSessionStatus status) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE download_session
        SET downloaded_size = :downloaded_size,
            checksum_sha256 = :checksum_sha256,
            status = :status,
            progress = CASE WHEN total_size > 0 THEN (:downloaded_size * 100.0 / total_size) ELSE progress END,
            updated_at = :updated_at,
            finished_at = CASE WHEN :is_finished = 1 THEN :finished_at ELSE finished_at END
        WHERE download_session_id = :download_session_id
    )");
    query.bindValue(":downloaded_size", qint64(downloadedSize));
    query.bindValue(":checksum_sha256", checksumSha256);
    query.bindValue(":status", TaskStatusText::toInt(status));
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":is_finished", status == DownloadSessionStatus::Finished ? 1 : 0);
    query.bindValue(":finished_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":download_session_id", taskUuid);
    return query.exec();
}

#ifndef DOWNLOADTASK_H
#define DOWNLOADTASK_H

#include "taskstatus.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlRecord>
#include <QString>
#include <QVariant>

struct DownloadTask
{
    QString task_uuid;
    int device_task_id;
    QString file_code;
    QString local_path;
    QString temp_path;
    qint64 downloaded_size;
    qint64 total_size;
    QString checksum_sha256;
    QDateTime created_at;
    QDateTime updated_at;
    QDateTime expire_time;
    DownloadSessionStatus status;
    QString error_message;

    DownloadTask()
        : device_task_id(-1)
        , downloaded_size(0)
        , total_size(0)
        , status(DownloadSessionStatus::Pending)
    {}

    bool isValid() const { return !task_uuid.isEmpty(); }

    static DownloadTask fromSqlRecord(const QSqlRecord& record)
    {
        DownloadTask task;
        task.task_uuid = record.value("download_session_id").toString();
        task.device_task_id = record.value("device_task_id").toInt();
        task.file_code = record.value("file_code").toString();
        task.local_path = record.value("local_path").toString();
        task.temp_path = record.value("temp_path").toString();
        task.downloaded_size = record.value("downloaded_size").toLongLong();
        task.total_size = record.value("total_size").toLongLong();
        task.checksum_sha256 = record.value("checksum_sha256").toString();
        task.created_at = record.value("started_at").toDateTime();
        task.updated_at = record.value("updated_at").toDateTime();
        task.expire_time = record.value("expire_time").toDateTime();
        task.status = TaskStatusText::downloadFromInt(record.value("status").toInt());
        task.error_message = record.value("error_message").toString();
        return task;
    }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json["download_session_id"] = task_uuid;
        json["device_task_id"] = device_task_id;
        json["file_code"] = file_code;
        json["local_path"] = local_path;
        json["temp_path"] = temp_path;
        json["downloaded_size"] = qint64(downloaded_size);
        json["total_size"] = qint64(total_size);
        json["checksum_sha256"] = checksum_sha256;
        json["started_at"] = created_at.toString(Qt::ISODate);
        json["updated_at"] = updated_at.toString(Qt::ISODate);
        json["expire_time"] = expire_time.toString(Qt::ISODate);
        json["status"] = TaskStatusText::toInt(status);
        json["error_message"] = error_message;
        return json;
    }
};

#endif // DOWNLOADTASK_H

#include "transferringtask.h"

TransferringTask::TransferringTask()
    : task_type(TaskType::Unknown)
    , priority(5)
    , file_size(0)
    , transferred_bytes(0)
    , status(TransferStatus::Pending)
    , current_step(CurrentSteps::Idle)
{}

TransferringTask::TransferringTask(const TaskBasicInfo& taskInfo, const FileInfo& fileInfo)
    : task_id(taskInfo.task_id)
    , file_id(fileInfo.file_code)
    , task_type(taskInfo.task_type)
    , description(taskInfo.description)
    , target_device_id(taskInfo.device_code)
    , priority(taskInfo.priority)
    , file_name(fileInfo.file_name)
    , file_size(fileInfo.file_size)
    , file_sha256(fileInfo.sha256_hash)
    , transferred_bytes(0)
    , status(TransferStatus::Pending)
    , current_step(CurrentSteps::Idle)
    , create_time(QDateTime::currentDateTime())
    , last_update_time(QDateTime::currentDateTime())
{}

bool TransferringTask::isValid() const
{
    return !task_id.isEmpty() && !file_id.isEmpty() && file_size > 0;
}

int TransferringTask::getProgressPercent() const
{
    if (file_size <= 0) return 0;
    return static_cast<int>((transferred_bytes * 100) / file_size);
}

QString TransferringTask::getStatusText() const
{
    if (status == TransferStatus::Downloading || status == TransferStatus::Uploading) {
        return QString("%1 (%2%)").arg(TransferStatus::toString(status)).arg(getProgressPercent());
    }
    return TransferStatus::toString(status);
}

bool TransferringTask::isRunning() const
{
    return status == TransferStatus::Downloading ||
           status == TransferStatus::Uploading ||
           status == TransferStatus::Installing;
}

bool TransferringTask::isFinished() const
{
    return status == TransferStatus::Succeeded ||
           status == TransferStatus::Failed ||
           status == TransferStatus::Cancelled;
}

void TransferringTask::updateProgress(qint64 bytesTransferred)
{
    transferred_bytes = bytesTransferred;
    last_update_time = QDateTime::currentDateTime();
}

void TransferringTask::setStatus(TransferStatus::Status newStatus)
{
    status = newStatus;
    last_update_time = QDateTime::currentDateTime();

    if (newStatus == TransferStatus::Downloading ||
        newStatus == TransferStatus::Uploading ||
        newStatus == TransferStatus::Installing) {
        if (start_time.isNull()) {
            start_time = QDateTime::currentDateTime();
        }
    }

    if (newStatus == TransferStatus::Succeeded ||
        newStatus == TransferStatus::Failed ||
        newStatus == TransferStatus::Cancelled) {
        end_time = QDateTime::currentDateTime();
    }
}

QJsonObject TransferringTask::toJson() const
{
    QJsonObject json;
    json["task_id"] = task_id;
    json["file_id"] = file_id;
    json["task_type"] = TaskType::toString(task_type);
    json["description"] = description;
    json["target_device_id"] = target_device_id;
    json["priority"] = priority;
    json["file_name"] = file_name;
    json["file_size"] = qint64(file_size);
    json["file_sha256"] = file_sha256;
    json["transferred_bytes"] = qint64(transferred_bytes);
    json["status"] = static_cast<int>(status);
    json["current_step"] = static_cast<int>(current_step);
    json["error_message"] = error_message;
    json["create_time"] = create_time.toString(Qt::ISODate);
    json["start_time"] = start_time.toString(Qt::ISODate);
    json["end_time"] = end_time.toString(Qt::ISODate);
    json["last_update_time"] = last_update_time.toString(Qt::ISODate);
    json["local_cache_path"] = local_cache_path;
    json["local_temp_path"] = local_temp_path;
    return json;
}

TransferringTask TransferringTask::fromJson(const QJsonObject& json)
{
    TransferringTask task;
    task.task_id = json["task_id"].toString();
    task.file_id = json["file_id"].toString();
    task.task_type = TaskType::fromString(json["task_type"].toString());
    task.description = json["description"].toString();
    task.target_device_id = json["target_device_id"].toString();
    task.priority = json["priority"].toInt(5);
    task.file_name = json["file_name"].toString();
    task.file_size = json["file_size"].toInteger();
    task.file_sha256 = json["file_sha256"].toString();
    task.transferred_bytes = json["transferred_bytes"].toInteger();
    task.status = static_cast<TransferStatus::Status>(json["status"].toInt());
    task.current_step = static_cast<CurrentSteps::Steps>(json["current_step"].toInt());
    task.error_message = json["error_message"].toString();
    task.create_time = QDateTime::fromString(json["create_time"].toString(), Qt::ISODate);
    task.start_time = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
    task.end_time = QDateTime::fromString(json["end_time"].toString(), Qt::ISODate);
    task.last_update_time = QDateTime::fromString(json["last_update_time"].toString(), Qt::ISODate);
    task.local_cache_path = json["local_cache_path"].toString();
    task.local_temp_path = json["local_temp_path"].toString();
    return task;
}

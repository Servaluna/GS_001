#include "devicetaskdao.h"

#include "../localdatabase/localdatabase.h"
#include "../logging/logger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

DeviceTask deviceTaskFromQuery(const QSqlQuery& query)
{
    return DeviceTask::fromSqlRecord(query.record());
}

QList<DeviceTask> readDeviceTasks(QSqlQuery& query)
{
    QList<DeviceTask> tasks;
    while (query.next()) {
        tasks.append(deviceTaskFromQuery(query));
    }
    return tasks;
}

}

bool DeviceTaskDAO::upsert(const DeviceTask& task) const
{
    if (!task.isValid()) {
        Logger::warn("DATABASE_SAVE_REJECTED", "无效设备任务，无法保存");
        return false;
    }

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO device_upgrade_task (
            device_task_id, server_device_task_id, aircraft_task_id, batch_id,
            owner_user_id, assigned_operator_user_id, aircraft_code,
            device_code, file_code, execution_order, status, progress,
            current_phase, retry_count, local_package_path, total_size,
            downloaded_size, transferred_size, start_time, last_update_time,
            finish_time, last_error
        ) VALUES (
            :device_task_id, :server_device_task_id, :aircraft_task_id, :batch_id,
            :owner_user_id, :assigned_operator_user_id, :aircraft_code,
            :device_code, :file_code, :execution_order, :status, :progress,
            :current_phase, :retry_count, :local_package_path, :total_size,
            :downloaded_size, :transferred_size, :start_time, :last_update_time,
            :finish_time, :last_error
        )
    )");
    query.bindValue(":device_task_id", task.device_task_id);
    query.bindValue(":server_device_task_id", task.server_device_task_id);
    query.bindValue(":aircraft_task_id", task.aircraft_task_id);
    query.bindValue(":batch_id", task.batch_id);
    query.bindValue(":owner_user_id", task.owner_user_id);
    query.bindValue(":assigned_operator_user_id", task.assigned_operator_user_id);
    query.bindValue(":aircraft_code", task.aircraft_code);
    query.bindValue(":device_code", task.device_code);
    query.bindValue(":file_code", task.file_code);
    query.bindValue(":execution_order", task.execution_order);
    query.bindValue(":status", TaskStatusText::toInt(task.status));
    query.bindValue(":progress", task.progress);
    query.bindValue(":current_phase", task.current_phase);
    query.bindValue(":retry_count", task.retry_count);
    query.bindValue(":local_package_path", task.local_package_path);
    query.bindValue(":total_size", qint64(task.total_size));
    query.bindValue(":downloaded_size", qint64(task.downloaded_size));
    query.bindValue(":transferred_size", qint64(task.transferred_size));
    query.bindValue(":start_time", task.start_time.toString(Qt::ISODate));
    query.bindValue(":last_update_time", task.last_update_time.toString(Qt::ISODate));
    query.bindValue(":finish_time", task.finish_time.toString(Qt::ISODate));
    query.bindValue(":last_error", task.last_error);

    if (!query.exec()) {
        Logger::error("DATABASE_SAVE_FAILED",
                      "保存设备任务失败",
                      {{"device_task_id", task.device_task_id}, {"error", query.lastError().text()}});
        return false;
    }
    return true;
}

bool DeviceTaskDAO::remove(int deviceTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("DELETE FROM device_upgrade_task WHERE device_task_id = :device_task_id");
    query.bindValue(":device_task_id", deviceTaskId);
    return query.exec();
}

DeviceTask DeviceTaskDAO::getById(int deviceTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM device_upgrade_task WHERE device_task_id = :device_task_id");
    query.bindValue(":device_task_id", deviceTaskId);

    if (query.exec() && query.next()) {
        return deviceTaskFromQuery(query);
    }
    return DeviceTask();
}

QList<DeviceTask> DeviceTaskDAO::getByAircraftTaskId(int aircraftTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        SELECT * FROM device_upgrade_task
        WHERE aircraft_task_id = :aircraft_task_id
        ORDER BY execution_order ASC, device_task_id ASC
    )");
    query.bindValue(":aircraft_task_id", aircraftTaskId);

    if (!query.exec()) {
        Logger::error("DATABASE_QUERY_FAILED",
                      "查询飞机下设备任务失败",
                      {{"aircraft_task_id", aircraftTaskId}, {"error", query.lastError().text()}});
        return {};
    }
    return readDeviceTasks(query);
}

QList<DeviceTask> DeviceTaskDAO::getByStatus(DeviceTaskStatus status) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM device_upgrade_task WHERE status = :status ORDER BY execution_order ASC");
    query.bindValue(":status", TaskStatusText::toInt(status));

    if (!query.exec()) {
        Logger::error("DATABASE_QUERY_FAILED",
                      "按状态查询设备任务失败",
                      {{"status", TaskStatusText::toInt(status)}, {"error", query.lastError().text()}});
        return {};
    }
    return readDeviceTasks(query);
}

bool DeviceTaskDAO::updateStatus(int deviceTaskId, DeviceTaskStatus status, double progress, const QString& errorMessage) const
{
    const int statusValue = TaskStatusText::toInt(status);
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET status = :status,
            progress = :progress,
            current_phase = :current_phase,
            last_error = :last_error,
            last_update_time = :last_update_time,
            finish_time = CASE WHEN :is_finished = 1 THEN :finish_time ELSE finish_time END
        WHERE device_task_id = :device_task_id
    )");
    query.bindValue(":status", statusValue);
    query.bindValue(":progress", progress);
    query.bindValue(":current_phase", TaskStatusText::devicePhase(status));
    query.bindValue(":last_error", errorMessage);
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":is_finished", TaskStatusText::isFinished(status) ? 1 : 0);
    query.bindValue(":finish_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":device_task_id", deviceTaskId);
    return query.exec();
}

bool DeviceTaskDAO::updateProgress(int deviceTaskId, qint64 downloadedSize, double progress) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET downloaded_size = :downloaded_size,
            progress = :progress,
            status = CASE WHEN :progress > 0 THEN :status ELSE status END,
            current_phase = CASE WHEN :progress > 0 THEN 'downloading' ELSE current_phase END,
            last_update_time = :last_update_time
        WHERE device_task_id = :device_task_id
    )");
    query.bindValue(":downloaded_size", qint64(downloadedSize));
    query.bindValue(":progress", progress);
    query.bindValue(":status", TaskStatusText::toInt(DeviceTaskStatus::Downloading));
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":device_task_id", deviceTaskId);
    return query.exec();
}

bool DeviceTaskDAO::updateTransferProgress(int deviceTaskId, qint64 transferredSize, double progress) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET transferred_size = :transferred_size,
            progress = :progress,
            status = CASE WHEN :progress > 0 THEN :status ELSE status END,
            current_phase = CASE WHEN :progress > 0 THEN 'transferring' ELSE current_phase END,
            last_update_time = :last_update_time
        WHERE device_task_id = :device_task_id
    )");
    query.bindValue(":transferred_size", qint64(transferredSize));
    query.bindValue(":progress", progress);
    query.bindValue(":status", TaskStatusText::toInt(DeviceTaskStatus::Transferring));
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":device_task_id", deviceTaskId);
    return query.exec();
}

bool DeviceTaskDAO::updateLocalPackagePath(int deviceTaskId, const QString& localPackagePath) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET local_package_path = :local_package_path,
            last_update_time = :last_update_time
        WHERE device_task_id = :device_task_id
    )");
    query.bindValue(":local_package_path", localPackagePath);
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":device_task_id", deviceTaskId);
    return query.exec();
}

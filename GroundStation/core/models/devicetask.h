#ifndef DEVICETASK_H
#define DEVICETASK_H

#include "taskstatus.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlRecord>
#include <QString>
#include <QVariant>

struct DeviceTask
{
    int device_task_id;
    int server_device_task_id;
    int aircraft_task_id;
    int batch_id;
    int owner_user_id;
    int assigned_operator_user_id;
    QString aircraft_code;
    QString device_code;
    QString file_code;
    int execution_order;
    DeviceTaskStatus status;
    double progress;
    int retry_count;
    qint64 transferred_size;
    qint64 total_size;
    qint64 downloaded_size;
    QString current_phase;
    QDateTime start_time;
    QDateTime last_update_time;
    QDateTime finish_time;
    QString last_error;
    QString local_package_path;

    DeviceTask()
        : device_task_id(-1)
        , server_device_task_id(-1)
        , aircraft_task_id(-1)
        , batch_id(-1)
        , owner_user_id(-1)
        , assigned_operator_user_id(-1)
        , execution_order(0)
        , status(DeviceTaskStatus::Waiting)
        , progress(0.0)
        , retry_count(0)
        , transferred_size(0)
        , total_size(0)
        , downloaded_size(0)
    {}

    bool isValid() const { return device_task_id > 0; }

    static DeviceTask fromSqlRecord(const QSqlRecord& record)
    {
        DeviceTask task;
        task.device_task_id = record.value("device_task_id").toInt();
        task.server_device_task_id = record.value("server_device_task_id").toInt();
        task.aircraft_task_id = record.value("aircraft_task_id").toInt();
        task.batch_id = record.value("batch_id").toInt();
        task.owner_user_id = record.value("owner_user_id").toInt();
        task.assigned_operator_user_id = record.value("assigned_operator_user_id").toInt();
        task.aircraft_code = record.value("aircraft_code").toString();
        task.device_code = record.value("device_code").toString();
        task.file_code = record.value("file_code").toString();
        task.execution_order = record.value("execution_order").toInt();
        task.status = TaskStatusText::deviceFromInt(record.value("status").toInt());
        task.progress = record.value("progress").toDouble();
        task.retry_count = record.value("retry_count").toInt();
        task.transferred_size = record.value("transferred_size").toLongLong();
        task.total_size = record.value("total_size").toLongLong();
        task.downloaded_size = record.value("downloaded_size").toLongLong();
        task.current_phase = record.value("current_phase").toString();
        task.start_time = record.value("start_time").toDateTime();
        task.last_update_time = record.value("last_update_time").toDateTime();
        task.finish_time = record.value("finish_time").toDateTime();
        task.last_error = record.value("last_error").toString();
        task.local_package_path = record.value("local_package_path").toString();
        return task;
    }

    static DeviceTask fromJson(const QJsonObject& json)
    {
        DeviceTask task;
        task.device_task_id = json["device_task_id"].toInt(-1);
        task.server_device_task_id = json["server_device_task_id"].toInt(task.device_task_id);
        task.aircraft_task_id = json["aircraft_task_id"].toInt(-1);
        task.batch_id = json["batch_id"].toInt(-1);
        task.owner_user_id = json["owner_user_id"].toInt(-1);
        task.assigned_operator_user_id = json["assigned_operator_user_id"].toInt(-1);
        task.aircraft_code = json["aircraft_code"].toString();
        task.device_code = json["device_code"].toString();
        task.file_code = json["file_code"].toString();
        task.execution_order = json["execution_order"].toInt(0);
        task.status = TaskStatusText::deviceFromInt(json["status"].toInt());
        task.progress = json["progress"].toDouble();
        task.retry_count = json["retry_count"].toInt();
        task.transferred_size = json["transferred_size"].toInteger();
        task.total_size = json["total_size"].toInteger();
        task.downloaded_size = json["downloaded_size"].toInteger();
        task.current_phase = TaskStatusText::normalizeDevicePhase(json["current_phase"].toString(), task.status);
        task.start_time = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
        task.last_update_time = QDateTime::fromString(json["last_update_time"].toString(), Qt::ISODate);
        task.finish_time = QDateTime::fromString(json["finish_time"].toString(), Qt::ISODate);
        task.last_error = json["last_error"].toString();
        task.local_package_path = json["local_package_path"].toString();
        return task;
    }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json["device_task_id"] = device_task_id;
        json["server_device_task_id"] = server_device_task_id;
        json["aircraft_task_id"] = aircraft_task_id;
        json["batch_id"] = batch_id;
        json["owner_user_id"] = owner_user_id;
        json["assigned_operator_user_id"] = assigned_operator_user_id;
        json["aircraft_code"] = aircraft_code;
        json["device_code"] = device_code;
        json["file_code"] = file_code;
        json["execution_order"] = execution_order;
        json["status"] = TaskStatusText::toInt(status);
        json["progress"] = progress;
        json["retry_count"] = retry_count;
        json["transferred_size"] = qint64(transferred_size);
        json["total_size"] = qint64(total_size);
        json["downloaded_size"] = qint64(downloaded_size);
        json["current_phase"] = current_phase;
        json["start_time"] = start_time.toString(Qt::ISODate);
        json["last_update_time"] = last_update_time.toString(Qt::ISODate);
        json["finish_time"] = finish_time.toString(Qt::ISODate);
        json["last_error"] = last_error;
        json["local_package_path"] = local_package_path;
        return json;
    }
};

#endif // DEVICETASK_H

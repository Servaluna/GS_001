#ifndef AIRCRAFTTASK_H
#define AIRCRAFTTASK_H

#include "taskstatus.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlRecord>
#include <QString>
#include <QVariant>

struct AircraftTask
{
    int aircraft_task_id;
    int batch_id;
    QString aircraft_code;
    int assigned_operator_user_id;
    DeviceTaskStatus status;
    double progress;
    QString current_phase;
    QString current_client_id;
    QDateTime start_time;
    QDateTime last_update_time;
    QDateTime finish_time;
    QString last_error;

    AircraftTask()
        : aircraft_task_id(-1)
        , batch_id(-1)
        , assigned_operator_user_id(-1)
        , status(DeviceTaskStatus::Waiting)
        , progress(0.0)
    {}

    bool isValid() const { return aircraft_task_id > 0; }

    static AircraftTask fromSqlRecord(const QSqlRecord& record)
    {
        AircraftTask task;
        task.aircraft_task_id = record.value("aircraft_task_id").toInt();
        task.batch_id = record.value("batch_id").toInt();
        task.aircraft_code = record.value("aircraft_code").toString();
        task.assigned_operator_user_id = record.value("assigned_operator_user_id").toInt();
        task.status = TaskStatusText::deviceFromInt(record.value("status").toInt());
        task.progress = record.value("progress").toDouble();
        task.current_phase = record.value("current_phase").toString();
        task.current_client_id = record.value("current_client_id").toString();
        task.start_time = record.value("start_time").toDateTime();
        task.last_update_time = record.value("last_update_time").toDateTime();
        task.finish_time = record.value("finish_time").toDateTime();
        task.last_error = record.value("last_error").toString();
        return task;
    }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json["aircraft_task_id"] = aircraft_task_id;
        json["batch_id"] = batch_id;
        json["aircraft_code"] = aircraft_code;
        json["assigned_operator_user_id"] = assigned_operator_user_id;
        json["status"] = TaskStatusText::toInt(status);
        json["progress"] = progress;
        json["current_phase"] = current_phase;
        json["current_client_id"] = current_client_id;
        json["start_time"] = start_time.toString(Qt::ISODate);
        json["last_update_time"] = last_update_time.toString(Qt::ISODate);
        json["finish_time"] = finish_time.toString(Qt::ISODate);
        json["last_error"] = last_error;
        return json;
    }
};

#endif // AIRCRAFTTASK_H

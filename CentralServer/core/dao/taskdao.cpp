#include "taskdao.h"

#include "../database/databasemanager.h"
#include "../logging/serverlogger.h"

#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QVariant>

namespace {

QString dateTimeToString(const QVariant& value)
{
    return value.toDateTime().toString(Qt::ISODate);
}

void bindUserFilter(QSqlQuery& query, int userId, int roleId)
{
    if (UserRole::isOperator(roleId)) {
        query.bindValue(":assigned_operator_user_id", userId);
    }
}

QString userWhereClause(int roleId)
{
    return UserRole::isOperator(roleId)
        ? QStringLiteral(" WHERE aut.assigned_operator_user_id = :assigned_operator_user_id ")
        : QStringLiteral(" ");
}

QVariant nullablePositiveInt(const QJsonObject& object, const QString& key)
{
    const int value = object.value(key).toInt(-1);
    return value > 0 ? QVariant(value) : QVariant();
}

}

QJsonArray TaskDAO::getAircraftTasksForUser(int userId, int roleId) const
{
    QJsonArray tasks;
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        ServerLogger::error("DATABASE_QUERY_REJECTED",
                            "数据库未连接，无法查询飞机任务",
                            {{"user_id", userId}, {"role_id", roleId}});
        return tasks;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(R"(
        SELECT
            aut.aircraft_task_id,
            aut.batch_id,
            a.aircraft_code,
            aut.assigned_operator_user_id,
            aut.status,
            aut.progress,
            aut.current_phase,
            aut.current_client_id,
            aut.start_time,
            aut.last_update_time,
            aut.finish_time,
            aut.last_error
        FROM aircraft_upgrade_task aut
        JOIN aircrafts a ON a.aircraft_id = aut.aircraft_id
    )") + userWhereClause(roleId) + QStringLiteral(" ORDER BY aut.aircraft_task_id ASC"));
    bindUserFilter(query, userId, roleId);

    if (!query.exec()) {
        ServerLogger::error("DATABASE_QUERY_FAILED",
                            "查询飞机任务失败",
                            {{"user_id", userId}, {"role_id", roleId}, {"error", query.lastError().text()}});
        return tasks;
    }

    while (query.next()) {
        QJsonObject task;
        task["aircraft_task_id"] = query.value("aircraft_task_id").toInt();
        task["batch_id"] = query.value("batch_id").toInt();
        task["aircraft_code"] = query.value("aircraft_code").toString();
        task["assigned_operator_user_id"] = query.value("assigned_operator_user_id").toInt();
        task["status"] = query.value("status").toInt();
        task["progress"] = query.value("progress").toDouble();
        task["current_phase"] = query.value("current_phase").toString();
        task["current_client_id"] = query.value("current_client_id").toString();
        task["start_time"] = dateTimeToString(query.value("start_time"));
        task["last_update_time"] = dateTimeToString(query.value("last_update_time"));
        task["finish_time"] = dateTimeToString(query.value("finish_time"));
        task["last_error"] = query.value("last_error").toString();
        tasks.append(task);
    }

    return tasks;
}

QJsonArray TaskDAO::getDeviceTasksForUser(int userId, int roleId) const
{
    QJsonArray tasks;
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        ServerLogger::error("DATABASE_QUERY_REJECTED",
                            "数据库未连接，无法查询设备任务",
                            {{"user_id", userId}, {"role_id", roleId}});
        return tasks;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(R"(
        SELECT
            dut.device_task_id,
            dut.aircraft_task_id,
            aut.batch_id,
            COALESCE(but.creator_user_id, aut.assigned_operator_user_id) AS owner_user_id,
            aut.assigned_operator_user_id,
            a.aircraft_code,
            d.device_code,
            f.file_code,
            dut.execution_order,
            dut.status,
            dut.progress,
            dut.retry_count,
            dut.transferred_size,
            COALESCE(dut.total_size, f.file_size) AS total_size,
            dut.start_time,
            dut.last_update_time,
            dut.finish_time,
            dut.last_error
        FROM device_upgrade_task dut
        JOIN aircraft_upgrade_task aut ON aut.aircraft_task_id = dut.aircraft_task_id
        JOIN batch_upgrade_task but ON but.batch_id = aut.batch_id
        JOIN aircrafts a ON a.aircraft_id = aut.aircraft_id
        JOIN devices d ON d.device_id = dut.device_id
        JOIN files f ON f.file_id = dut.file_id
    )") + userWhereClause(roleId) + QStringLiteral(" ORDER BY dut.aircraft_task_id ASC, dut.execution_order ASC, dut.device_task_id ASC"));
    bindUserFilter(query, userId, roleId);

    if (!query.exec()) {
        ServerLogger::error("DATABASE_QUERY_FAILED",
                            "查询设备任务失败",
                            {{"user_id", userId}, {"role_id", roleId}, {"error", query.lastError().text()}});
        return tasks;
    }

    while (query.next()) {
        QJsonObject task;
        const int deviceTaskId = query.value("device_task_id").toInt();
        const int status = query.value("status").toInt();
        task["device_task_id"] = deviceTaskId;
        task["server_device_task_id"] = deviceTaskId;
        task["aircraft_task_id"] = query.value("aircraft_task_id").toInt();
        task["batch_id"] = query.value("batch_id").toInt();
        task["owner_user_id"] = query.value("owner_user_id").toInt();
        task["assigned_operator_user_id"] = query.value("assigned_operator_user_id").toInt();
        task["aircraft_code"] = query.value("aircraft_code").toString();
        task["device_code"] = query.value("device_code").toString();
        task["file_code"] = query.value("file_code").toString();
        task["execution_order"] = query.value("execution_order").toInt();
        task["status"] = status;
        task["progress"] = query.value("progress").toDouble();
        task["retry_count"] = query.value("retry_count").toInt();
        task["transferred_size"] = qint64(query.value("transferred_size").toLongLong());
        task["total_size"] = qint64(query.value("total_size").toLongLong());
        task["downloaded_size"] = 0;
        task["current_phase"] = TaskStatusText::devicePhase(TaskStatusText::deviceFromInt(status));
        task["start_time"] = dateTimeToString(query.value("start_time"));
        task["last_update_time"] = dateTimeToString(query.value("last_update_time"));
        task["finish_time"] = dateTimeToString(query.value("finish_time"));
        task["last_error"] = query.value("last_error").toString();
        task["local_package_path"] = "";
        tasks.append(task);
    }

    return tasks;
}

bool TaskDAO::updateAircraftTaskStatus(const QJsonObject& statusData) const
{
    const int aircraftTaskId = statusData["aircraft_task_id"].toInt(-1);
    if (aircraftTaskId <= 0) {
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        return false;
    }

    const int status = statusData["status"].toInt(0);
    const double progress = statusData["progress"].toDouble(0.0);
    const QString phase = statusData["current_phase"].toString();
    const QString error = statusData["last_error"].toString();
    const bool finished = status == TaskStatusText::toInt(DeviceTaskStatus::Success) ||
                          status == TaskStatusText::toInt(DeviceTaskStatus::Failed);

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE aircraft_upgrade_task
        SET status = :status,
            progress = :progress,
            current_phase = :current_phase,
            last_error = :last_error,
            start_time = CASE WHEN start_time IS NULL AND :status <> 0 THEN CURRENT_TIMESTAMP ELSE start_time END,
            last_update_time = CURRENT_TIMESTAMP,
            finish_time = CASE WHEN :finished = 1 THEN CURRENT_TIMESTAMP ELSE finish_time END
        WHERE aircraft_task_id = :aircraft_task_id
    )");
    query.bindValue(":aircraft_task_id", aircraftTaskId);
    query.bindValue(":status", status);
    query.bindValue(":progress", progress);
    query.bindValue(":current_phase", phase);
    query.bindValue(":last_error", error);
    query.bindValue(":finished", finished ? 1 : 0);

    if (!query.exec()) {
        ServerLogger::error("DATABASE_UPDATE_FAILED",
                            "更新飞机任务状态失败",
                            {{"aircraft_task_id", aircraftTaskId}, {"error", query.lastError().text()}});
        return false;
    }
    return true;
}

bool TaskDAO::updateDeviceTaskStatus(const QJsonObject& statusData) const
{
    const int deviceTaskId = statusData["device_task_id"].toInt(-1);
    if (deviceTaskId <= 0) {
        return true;
    }

    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        return false;
    }

    const int status = statusData["device_status"].toInt(statusData["status"].toInt(0));
    const double progress = statusData["device_progress"].toDouble(statusData["progress"].toDouble(0.0));
    const qint64 transferredSize = statusData["transferred_size"].toInteger(0);
    const qint64 totalSize = statusData["total_size"].toInteger(0);
    const QString error = statusData["last_error"].toString();
    const bool finished = status == TaskStatusText::toInt(DeviceTaskStatus::Success) ||
                          status == TaskStatusText::toInt(DeviceTaskStatus::Failed);

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET status = :status,
            progress = :progress,
            transferred_size = CASE WHEN :transferred_size >= 0 THEN :transferred_size ELSE transferred_size END,
            total_size = CASE WHEN :total_size > 0 THEN :total_size ELSE total_size END,
            last_error = :last_error,
            start_time = CASE WHEN start_time IS NULL AND :status <> 0 THEN CURRENT_TIMESTAMP ELSE start_time END,
            last_update_time = CURRENT_TIMESTAMP,
            finish_time = CASE WHEN :finished = 1 THEN CURRENT_TIMESTAMP ELSE finish_time END
        WHERE device_task_id = :device_task_id
    )");
    query.bindValue(":device_task_id", deviceTaskId);
    query.bindValue(":status", status);
    query.bindValue(":progress", progress);
    query.bindValue(":transferred_size", transferredSize);
    query.bindValue(":total_size", totalSize);
    query.bindValue(":last_error", error);
    query.bindValue(":finished", finished ? 1 : 0);

    if (!query.exec()) {
        ServerLogger::error("DATABASE_UPDATE_FAILED",
                            "更新设备任务状态失败",
                            {{"device_task_id", deviceTaskId}, {"error", query.lastError().text()}});
        return false;
    }
    return true;
}

bool TaskDAO::insertTaskAuditLog(const QJsonObject& statusData) const
{
    QSqlDatabase db = DatabaseManager::instance().getDatabase();
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO audit_log (
            event_type, event_level, operator_user_id, session_id,
            batch_id, aircraft_task_id, device_task_id,
            event_message, event_detail, ip_address
        ) VALUES (
            :event_type, :event_level, :operator_user_id, :session_id,
            :batch_id, :aircraft_task_id, :device_task_id,
            :event_message, :event_detail, :ip_address
        )
    )");
    query.bindValue(":event_type", statusData["event_type"].toString("TASK_STATUS_UPDATE"));
    query.bindValue(":event_level", statusData["event_level"].toString("INFO"));
    query.bindValue(":operator_user_id", nullablePositiveInt(statusData, "operator_user_id"));
    query.bindValue(":session_id", statusData["session_id"].toString());
    query.bindValue(":batch_id", nullablePositiveInt(statusData, "batch_id"));
    query.bindValue(":aircraft_task_id", nullablePositiveInt(statusData, "aircraft_task_id"));
    query.bindValue(":device_task_id", nullablePositiveInt(statusData, "device_task_id"));
    query.bindValue(":event_message", statusData["event_message"].toString());
    query.bindValue(":event_detail", QString::fromUtf8(QJsonDocument(statusData).toJson(QJsonDocument::Compact)));
    query.bindValue(":ip_address", statusData["ip_address"].toString());

    if (!query.exec()) {
        ServerLogger::warn("DATABASE_INSERT_FAILED",
                           "写入任务状态审计日志失败",
                           {{"error", query.lastError().text()}});
        return false;
    }
    return true;
}

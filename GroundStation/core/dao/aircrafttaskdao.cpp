#include "aircrafttaskdao.h"

#include "../localdatabase/localdatabase.h"
#include "../logging/logger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

AircraftTask aircraftTaskFromQuery(const QSqlQuery& query)
{
    return AircraftTask::fromSqlRecord(query.record());
}

QList<AircraftTask> readAircraftTasks(QSqlQuery& query)
{
    QList<AircraftTask> tasks;
    while (query.next()) {
        tasks.append(aircraftTaskFromQuery(query));
    }
    return tasks;
}

QString aggregateSelectSql()
{
    return QStringLiteral(R"(
        SELECT
            aircraft_task_id,
            MAX(batch_id) AS batch_id,
            MAX(aircraft_code) AS aircraft_code,
            MAX(assigned_operator_user_id) AS assigned_operator_user_id,
            CASE
                WHEN SUM(CASE WHEN status = 6 THEN 1 ELSE 0 END) > 0 THEN 6
                WHEN SUM(CASE WHEN status = 4 THEN 1 ELSE 0 END) > 0 THEN 4
                WHEN SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) > 0 THEN 3
                WHEN SUM(CASE WHEN status = 2 THEN 1 ELSE 0 END) > 0 THEN 2
                WHEN SUM(CASE WHEN status = 1 THEN 1 ELSE 0 END) > 0 THEN 1
                WHEN COUNT(*) > 0 AND SUM(CASE WHEN status = 5 THEN 1 ELSE 0 END) = COUNT(*) THEN 5
                ELSE 0
            END AS status,
            COALESCE(AVG(progress), 0) AS progress,
            MAX(current_phase) AS current_phase,
            '' AS current_client_id,
            MIN(start_time) AS start_time,
            MAX(last_update_time) AS last_update_time,
            MAX(finish_time) AS finish_time,
            MAX(last_error) AS last_error
        FROM device_upgrade_task
    )");
}

}

bool AircraftTaskDAO::upsert(const AircraftTask& task) const
{
    if (!task.isValid()) {
        Logger::warn("DATABASE_SAVE_REJECTED", "无效飞机任务，无法保存");
        return false;
    }

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET batch_id = :batch_id,
            aircraft_code = :aircraft_code,
            assigned_operator_user_id = :assigned_operator_user_id,
            last_update_time = :last_update_time
        WHERE aircraft_task_id = :aircraft_task_id
    )");
    query.bindValue(":aircraft_task_id", task.aircraft_task_id);
    query.bindValue(":batch_id", task.batch_id);
    query.bindValue(":aircraft_code", task.aircraft_code);
    query.bindValue(":assigned_operator_user_id", task.assigned_operator_user_id);
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        Logger::error("DATABASE_SAVE_FAILED",
                      "更新飞机任务聚合信息失败",
                      {{"aircraft_task_id", task.aircraft_task_id}, {"error", query.lastError().text()}});
        return false;
    }
    return true;
}

bool AircraftTaskDAO::remove(int aircraftTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("DELETE FROM device_upgrade_task WHERE aircraft_task_id = :aircraft_task_id");
    query.bindValue(":aircraft_task_id", aircraftTaskId);
    return query.exec();
}

AircraftTask AircraftTaskDAO::getById(int aircraftTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(aggregateSelectSql() + " WHERE aircraft_task_id = :aircraft_task_id GROUP BY aircraft_task_id");
    query.bindValue(":aircraft_task_id", aircraftTaskId);

    if (query.exec() && query.next()) {
        return aircraftTaskFromQuery(query);
    }
    return AircraftTask();
}

QList<AircraftTask> AircraftTaskDAO::getByAssignedOperator(int operatorUserId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(aggregateSelectSql() + R"(
        WHERE assigned_operator_user_id = :assigned_operator_user_id
        GROUP BY aircraft_task_id
        ORDER BY aircraft_task_id ASC
    )");
    query.bindValue(":assigned_operator_user_id", operatorUserId);

    if (!query.exec()) {
        Logger::error("DATABASE_QUERY_FAILED",
                      "查询操作员飞机任务失败",
                      {{"operator_user_id", operatorUserId}, {"error", query.lastError().text()}});
        return {};
    }
    return readAircraftTasks(query);
}

QList<AircraftTask> AircraftTaskDAO::getAll() const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    if (!query.exec(aggregateSelectSql() + " GROUP BY aircraft_task_id ORDER BY aircraft_task_id ASC")) {
        Logger::error("DATABASE_QUERY_FAILED",
                      "查询飞机任务失败",
                      {{"error", query.lastError().text()}});
        return {};
    }
    return readAircraftTasks(query);
}

bool AircraftTaskDAO::updateStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase) const
{
    const int statusValue = TaskStatusText::toInt(status);
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE device_upgrade_task
        SET status = CASE
                WHEN :status = 0 THEN 0
                WHEN :status = 5 THEN 5
                ELSE status
            END,
            progress = CASE
                WHEN :status = 0 THEN 0
                WHEN :status = 5 THEN 100
                ELSE progress
            END,
            current_phase = :current_phase,
            last_update_time = :last_update_time,
            finish_time = CASE WHEN :status = 5 THEN :finish_time ELSE finish_time END
        WHERE aircraft_task_id = :aircraft_task_id
    )");
    query.bindValue(":status", statusValue);
    query.bindValue(":progress", progress);
    query.bindValue(":current_phase", phase.isEmpty() ? TaskStatusText::devicePhase(status) : phase);
    query.bindValue(":last_update_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":finish_time", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":aircraft_task_id", aircraftTaskId);
    return query.exec();
}

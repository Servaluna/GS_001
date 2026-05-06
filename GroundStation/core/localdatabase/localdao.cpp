#include "localdao.h"
#include "localdatabase.h"

#include <QDateTime>
#include <QSqlError>

LocalDAO::LocalDAO() {}

bool LocalDAO::insert(const TransferringTask& task)
{
    if (!task.isValid()) {
        qWarning() << "任务无效，无法插入";
        return false;
    }

    QString sql = R"(
        INSERT INTO transferring_tasks (
            task_id, file_id, task_type, description, target_device_id,
            priority, file_name, file_size, file_sha256, transferred_bytes,
            status, current_step, error_message,
            create_time, start_time, end_time, last_update_time,
            local_cache_path, local_temp_path
        ) VALUES (
            :task_id, :file_id, :task_type, :description, :target_device_id,
            :priority, :file_name, :file_size, :file_sha256, :transferred_bytes,
            :status, :current_step, :error_message,
            :create_time, :start_time, :end_time, :last_update_time,
            :local_cache_path, :local_temp_path
        )
    )";

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);

    query.bindValue(":task_id", task.task_id);
    query.bindValue(":file_id", task.file_id);
    query.bindValue(":task_type", static_cast<int>(task.task_type));
    query.bindValue(":description", task.description);
    query.bindValue(":target_device_id", task.target_device_id);
    query.bindValue(":priority", task.priority);
    query.bindValue(":file_name", task.file_name);
    query.bindValue(":file_size", qint64(task.file_size));
    query.bindValue(":file_sha256", task.file_sha256);
    query.bindValue(":transferred_bytes", qint64(task.transferred_bytes));
    query.bindValue(":status", static_cast<int>(task.status));
    query.bindValue(":current_step", static_cast<int>(task.current_step));
    query.bindValue(":error_message", task.error_message);
    query.bindValue(":create_time", task.create_time.toSecsSinceEpoch());
    query.bindValue(":start_time", task.start_time.toSecsSinceEpoch());
    query.bindValue(":end_time", task.end_time.toSecsSinceEpoch());
    query.bindValue(":last_update_time", task.last_update_time.toSecsSinceEpoch());
    query.bindValue(":local_cache_path", task.local_cache_path);
    query.bindValue(":local_temp_path", task.local_temp_path);

    if (!query.exec()) {
        qWarning() << "插入任务失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool LocalDAO::update(const TransferringTask& task)
{
    QString sql = R"(
        UPDATE transferring_tasks SET
            task_type = :task_type,
            description = :description,
            target_device_id = :target_device_id,
            priority = :priority,
            file_name = :file_name,
            file_size = :file_size,
            file_sha256 = :file_sha256,
            transferred_bytes = :transferred_bytes,
            status = :status,
            current_step = :current_step,
            error_message = :error_message,
            start_time = :start_time,
            end_time = :end_time,
            last_update_time = :last_update_time,
            local_cache_path = :local_cache_path,
            local_temp_path = :local_temp_path
        WHERE task_id = :task_id
    )";

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);

    query.bindValue(":task_id", task.task_id);
    query.bindValue(":task_type", static_cast<int>(task.task_type));
    query.bindValue(":description", task.description);
    query.bindValue(":target_device_id", task.target_device_id);
    query.bindValue(":priority", task.priority);
    query.bindValue(":file_name", task.file_name);
    query.bindValue(":file_size", qint64(task.file_size));
    query.bindValue(":file_sha256", task.file_sha256);
    query.bindValue(":transferred_bytes", qint64(task.transferred_bytes));
    query.bindValue(":status", static_cast<int>(task.status));
    query.bindValue(":current_step", static_cast<int>(task.current_step));
    query.bindValue(":error_message", task.error_message);
    query.bindValue(":start_time", task.start_time.toSecsSinceEpoch());
    query.bindValue(":end_time", task.end_time.toSecsSinceEpoch());
    query.bindValue(":last_update_time", task.last_update_time.toSecsSinceEpoch());
    query.bindValue(":local_cache_path", task.local_cache_path);
    query.bindValue(":local_temp_path", task.local_temp_path);

    if (!query.exec()) {
        qWarning() << "更新任务失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool LocalDAO::remove(int id)
{
    QString sql = "DELETE FROM transferring_tasks WHERE id = :id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":id", id);

    return query.exec();
}

bool LocalDAO::removeByTaskId(const QString& taskId)
{
    QString sql = "DELETE FROM transferring_tasks WHERE task_id = :task_id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":task_id", taskId);

    return query.exec();
}

void LocalDAO::clearCompleted()
{
    QString sql = "DELETE FROM transferring_tasks WHERE status IN (5, 6, 7)";
    LocalDatabase::getInstance()->executeQuery(sql);
}

void LocalDAO::clearAll()
{
    LocalDatabase::getInstance()->executeQuery("DELETE FROM transferring_tasks");
}

TransferringTask LocalDAO::getTransferringTaskById(int id)
{
    QString sql = QString("SELECT * FROM transferring_tasks WHERE id = %1").arg(id);
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return rowToTask(query);
    }

    return TransferringTask();
}

TransferringTask LocalDAO::getTransferringTaskById(const QString& taskId)
{
    QString sql = "SELECT * FROM transferring_tasks WHERE task_id = :task_id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":task_id", taskId);

    if (query.exec() && query.next()) {
        return rowToTask(query);
    }

    return TransferringTask();
}

QList<TransferringTask> LocalDAO::getAll()
{
    QList<TransferringTask> tasks;
    QString sql = "SELECT * FROM transferring_tasks ORDER BY priority DESC, create_time ASC";
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    while (query.next()) {
        tasks.append(rowToTask(query));
    }

    return tasks;
}

QList<TransferringTask> LocalDAO::getByStatus(TransferStatus::Status status)
{
    QList<TransferringTask> tasks;
    QString sql = "SELECT * FROM transferring_tasks WHERE status = :status ORDER BY priority DESC, create_time ASC";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":status", static_cast<int>(status));

    if (query.exec()) {
        while (query.next()) {
            tasks.append(rowToTask(query));
        }
    }

    return tasks;
}

QList<TransferringTask> LocalDAO::getPendingTasks()
{
    return getByStatus(TransferStatus::Pending);
}

QList<TransferringTask> LocalDAO::getRunningTasks()
{
    QList<TransferringTask> tasks;
    QString sql = "SELECT * FROM transferring_tasks WHERE status IN (1, 2, 3) ORDER BY priority DESC, create_time ASC";
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    while (query.next()) {
        tasks.append(rowToTask(query));
    }

    return tasks;
}

TransferringTask LocalDAO::getCurrentRunningTask()
{
    QString sql = "SELECT * FROM transferring_tasks WHERE status IN (1, 2, 3) ORDER BY priority DESC, create_time ASC LIMIT 1";
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return rowToTask(query);
    }

    return TransferringTask();
}

int LocalDAO::getPendingCount()
{
    QString sql = "SELECT COUNT(*) FROM transferring_tasks WHERE status = 0";
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int LocalDAO::getRunningCount()
{
    QString sql = "SELECT COUNT(*) FROM transferring_tasks WHERE status IN (1, 2, 3)";
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int LocalDAO::getCompletedCount()
{
    QString sql = QString("SELECT COUNT(*) FROM transferring_tasks WHERE status = %1")
                      .arg(static_cast<int>(TransferStatus::Succeeded));
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int LocalDAO::getFailedCount()
{
    QString sql = QString("SELECT COUNT(*) FROM transferring_tasks WHERE status = %1")
                      .arg(static_cast<int>(TransferStatus::Failed));
    QSqlQuery query = LocalDatabase::getInstance()->executeQueryWithResult(sql);

    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool LocalDAO::updateStatus(const QString& taskId, TransferStatus::Status status)
{
    QString sql = "UPDATE transferring_tasks SET status = :status, last_update_time = :last_update_time WHERE task_id = :task_id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":status", static_cast<int>(status));
    query.bindValue(":last_update_time", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":task_id", taskId);

    return query.exec();
}

bool LocalDAO::updateProgress(const QString& taskId, qint64 transferredBytes)
{
    QString sql = "UPDATE transferring_tasks SET transferred_bytes = :transferred_bytes, last_update_time = :last_update_time WHERE task_id = :task_id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":transferred_bytes", qint64(transferredBytes));
    query.bindValue(":last_update_time", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":task_id", taskId);

    return query.exec();
}

bool LocalDAO::updateCurrentStep(const QString& taskId, CurrentSteps::Steps step)
{
    QString sql = "UPDATE transferring_tasks SET current_step = :current_step, last_update_time = :last_update_time WHERE task_id = :task_id";
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(sql);
    query.bindValue(":current_step", static_cast<int>(step));
    query.bindValue(":last_update_time", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":task_id", taskId);

    return query.exec();
}

TransferringTask LocalDAO::rowToTask(const QSqlQuery& query)
{
    TransferringTask task;
    task.task_id = query.value("task_id").toString();
    task.file_id = query.value("file_id").toString();
    task.task_type = static_cast<TaskType::Type>(query.value("task_type").toInt());
    task.description = query.value("description").toString();
    task.target_device_id = query.value("target_device_id").toString();
    task.priority = query.value("priority").toInt();
    task.file_name = query.value("file_name").toString();
    task.file_size = query.value("file_size").toLongLong();
    task.file_sha256 = query.value("file_sha256").toString();
    task.transferred_bytes = query.value("transferred_bytes").toLongLong();
    task.status = static_cast<TransferStatus::Status>(query.value("status").toInt());
    task.current_step = static_cast<CurrentSteps::Steps>(query.value("current_step").toInt());
    task.error_message = query.value("error_message").toString();

    qint64 createTime = query.value("create_time").toLongLong();
    qint64 startTime = query.value("start_time").toLongLong();
    qint64 endTime = query.value("end_time").toLongLong();
    qint64 lastUpdateTime = query.value("last_update_time").toLongLong();

    task.create_time = QDateTime::fromSecsSinceEpoch(createTime);
    task.start_time = startTime > 0 ? QDateTime::fromSecsSinceEpoch(startTime) : QDateTime();
    task.end_time = endTime > 0 ? QDateTime::fromSecsSinceEpoch(endTime) : QDateTime();
    task.last_update_time = QDateTime::fromSecsSinceEpoch(lastUpdateTime);
    task.local_cache_path = query.value("local_cache_path").toString();
    task.local_temp_path = query.value("local_temp_path").toString();

    return task;
}

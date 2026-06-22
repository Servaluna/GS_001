#include "taskservice.h"

#include "../logging/logger.h"
#include "../localdatabase/localdatabase.h"
#include "../repository/taskrepository.h"
#include "../domain/download/downloadmanager.h"
#include "../domain/scheduler/taskscheduler.h"
#include "../domain/state/taskstatemachine.h"
#include "../domain/transfer/transfermanager.h"
#include "../network/serverconnector.h"

#include "models.h"

#include <QEventLoop>
#include <QJsonValue>
#include <QTimer>

class DeviceConnector;
class ServerConnector;

TaskService::TaskService(QObject *parent)
    : QObject{parent}
    , m_repository(new TaskRepository())
    , m_downloadManager(new DownloadManager(this))
    , m_transferManager(new TransferManager(this))
    , m_stateMachine(new TaskStateMachine(this))
    , m_scheduler(new TaskScheduler(this))
    , m_initialized(false)
    , m_lastSyncUserId(-1)
    , m_lastSyncRoleId(UserRole::Unknown)
{}

TaskService::~TaskService()
{
    delete m_repository;
}

bool TaskService::init(ServerConnector* serverConnector, DeviceConnector* deviceConnector)
{
    if (!serverConnector || !deviceConnector) {
        Logger::error("TASK_SERVICE_INIT_FAILED", "TaskService 初始化失败，依赖为空");
        return false;
    }

    if (!m_downloadManager->init(serverConnector, m_repository)) {
        return false;
    }
    if (!m_transferManager->init(deviceConnector)) {
        return false;
    }
    if (!m_stateMachine->init(m_repository)) {
        return false;
    }
    if (!m_scheduler->init(m_repository, m_downloadManager, m_transferManager, m_stateMachine)) {
        return false;
    }

    connect(m_scheduler, &TaskScheduler::taskStarted,
            this, &TaskService::taskStarted);
    connect(m_scheduler, &TaskScheduler::taskProgressUpdated,
            this, &TaskService::taskProgressUpdated);
    connect(m_scheduler, &TaskScheduler::taskFinished,
            this, &TaskService::taskFinished);
    connect(m_scheduler, &TaskScheduler::queueStatusChanged,
            this, &TaskService::queueStatusChanged);

    connect(serverConnector, &ServerConnector::currentUserTasksReceived,
            this, [this](const QJsonArray& aircraftTasks, const QJsonArray& deviceTasks) {
                const bool saved = saveCurrentUserTasks(aircraftTasks, deviceTasks);
                emit currentUserTasksSynced(saved, aircraftTasks.size(), deviceTasks.size());
            });

    m_initialized = true;
    Logger::info("TASK_SERVICE_READY", "任务服务初始化完成");
    return true;
}

void TaskService::start()
{
    if (m_initialized) {
        m_scheduler->start();
        Logger::info("TASK_SERVICE_START", "任务服务启动");
    }
}

void TaskService::stop()
{
    if (m_initialized) {
        m_scheduler->stop();
        Logger::info("TASK_SERVICE_STOP", "任务服务停止");
    }
}

bool TaskService::syncTasksForUser(int userId, int roleId, int timeoutMs)
{
    if (!m_initialized) {
        Logger::warn("TASK_SYNC_REJECTED", "任务服务未初始化", {{"user_id", userId}, {"role_id", roleId}});
        return false;
    }

    ServerConnector& server = ServerConnector::instance();
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    bool received = false;
    bool saved = false;
    m_lastSyncUserId = userId;
    m_lastSyncRoleId = roleId;

    Logger::info("TASK_SYNC_START",
                 "开始同步当前登录用户任务",
                 {{"user_id", userId}, {"role_id", roleId}, {"timeout_ms", timeoutMs}});

    QMetaObject::Connection tasksConnection;
    tasksConnection = connect(this, &TaskService::currentUserTasksSynced,
                              &loop,
                              [&](bool success, int aircraftTaskCount, int deviceTaskCount) {
        received = true;
        saved = success;
        Logger::info("TASK_SYNC_WAIT_FINISHED",
                     "登录等待期间收到任务同步结果",
                     {{"user_id", userId},
                      {"role_id", roleId},
                      {"aircraft_task_count", aircraftTaskCount},
                      {"device_task_count", deviceTaskCount},
                      {"success", success}});
        loop.quit();
    });

    QMetaObject::Connection errorConnection;
    errorConnection = connect(&server, &ServerConnector::errorOccurred,
                              this,
                              [&](const QString& error) {
        Logger::warn("TASK_SYNC_FAILED", "任务同步失败", {{"user_id", userId}, {"role_id", roleId}, {"error", error}});
        loop.quit();
    });

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    if (!server.requestCurrentUserTasks(userId, roleId)) {
        disconnect(tasksConnection);
        disconnect(errorConnection);
        return false;
    }

    timer.start(timeoutMs);
    loop.exec();

    disconnect(tasksConnection);
    disconnect(errorConnection);

    if (!received) {
        Logger::warn("TASK_SYNC_TIMEOUT",
                     "任务同步等待超时，响应晚到时仍会继续写入本地缓存",
                     {{"user_id", userId}, {"role_id", roleId}, {"timeout_ms", timeoutMs}});
        return false;
    }

    m_repository->clearCache();
    Logger::info("TASK_SYNC_RESULT",
                 saved ? "任务同步结果：本地保存成功" : "任务同步结果：部分或全部本地保存失败",
                 {{"user_id", userId}, {"role_id", roleId}, {"success", saved}});
    return saved;
}

bool TaskService::saveCurrentUserTasks(const QJsonArray& aircraftTasks, const QJsonArray& deviceTasks)
{
    bool saved = true;
    int parsedCount = 0;
    int invalidCount = 0;
    int savedCount = 0;
    int failedCount = 0;

    Logger::info("TASK_SYNC_PAYLOAD_RECEIVED",
                 "收到任务同步数据",
                 {{"user_id", m_lastSyncUserId},
                  {"role_id", m_lastSyncRoleId},
                  {"aircraft_task_count", aircraftTasks.size()},
                  {"device_task_count", deviceTasks.size()}});

    for (const QJsonValue& value : deviceTasks) {
        const DeviceTask task = DeviceTask::fromJson(value.toObject());
        if (!task.isValid()) {
            ++invalidCount;
            saved = false;
            Logger::warn("TASK_SYNC_DEVICE_INVALID",
                         "服务器返回的设备任务无效，已跳过",
                         {{"raw_task", value.toObject()}});
            continue;
        }

        ++parsedCount;
        if (m_repository->saveDeviceTask(task)) {
            ++savedCount;
        } else {
            ++failedCount;
            saved = false;
            Logger::error("TASK_SYNC_DEVICE_SAVE_FAILED",
                          "设备任务写入本地数据库失败",
                          {{"device_task_id", task.device_task_id},
                           {"aircraft_task_id", task.aircraft_task_id},
                           {"batch_id", task.batch_id},
                           {"aircraft_code", task.aircraft_code},
                           {"device_code", task.device_code},
                           {"file_code", task.file_code},
                           {"owner_user_id", task.owner_user_id},
                           {"assigned_operator_user_id", task.assigned_operator_user_id}});
        }
    }

    m_repository->clearCache();
    Logger::info("TASK_SYNC_FINISHED",
                 "任务同步完成",
                 {{"user_id", m_lastSyncUserId},
                  {"role_id", m_lastSyncRoleId},
                  {"aircraft_task_count", aircraftTasks.size()},
                  {"device_task_count", deviceTasks.size()},
                  {"parsed_device_task_count", parsedCount},
                  {"invalid_device_task_count", invalidCount},
                  {"saved_device_task_count", savedCount},
                  {"failed_device_task_count", failedCount}});
    return saved;
}

QList<AircraftTask> TaskService::getExecutableAircraftTasksForUser(int userId, int roleId)
{
    if (!m_initialized) {
        Logger::warn("TASK_LIST_QUERY_REJECTED",
                     "任务服务未初始化，无法读取可执行任务",
                     {{"user_id", userId}, {"role_id", roleId}});
        return {};
    }

    Logger::info("TASK_LIST_QUERY_START",
                 "开始读取当前用户可执行飞机任务",
                 {{"user_id", userId}, {"role_id", roleId}});

    QList<AircraftTask> tasks;
    if (UserRole::isOperator(roleId)) {
        tasks = m_repository->getAircraftTasksForCurrentOperator(userId);
    } else {
        tasks = m_repository->getAllAircraftTasks();
    }

    Logger::info("TASK_LIST_QUERY_FINISHED",
                 "读取当前用户可执行飞机任务完成",
                 {{"user_id", userId}, {"role_id", roleId}, {"aircraft_task_count", tasks.size()}});
    return tasks;
}

bool TaskService::clearLocalTaskData()
{
    if (!m_initialized) {
        Logger::warn("TASK_LOCAL_CLEAR_REJECTED", "任务服务未初始化，无法清空本地任务数据");
        return false;
    }

    if (!LocalDatabase::getInstance()->clearBusinessData()) {
        Logger::error("TASK_LOCAL_CLEAR_FAILED", "清空本地任务数据库失败");
        return false;
    }

    m_repository->clearCache();
    Logger::info("TASK_LOCAL_CLEAR_FINISHED", "已清空当前用户本地任务数据和缓存");
    return true;
}

bool TaskService::startTask(const QString& taskId)
{
    if (!m_initialized) {
        Logger::warn("TASK_START_REJECTED", "任务服务未初始化，无法启动任务", {{"aircraft_task_id", taskId}});
        return false;
    }

    Logger::info("TASK_START_REQUEST", QString("请求启动飞机任务 %1").arg(taskId), {{"aircraft_task_id", taskId}});
    return m_scheduler->startTask(taskId);
}

bool TaskService::pauseTask(const QString& taskId)
{
    if (!m_initialized) {
        Logger::warn("TASK_PAUSE_REJECTED", "任务服务未初始化，无法暂停任务", {{"aircraft_task_id", taskId}});
        return false;
    }

    Logger::info("TASK_PAUSE_REQUEST", QString("请求暂停飞机任务 %1").arg(taskId), {{"aircraft_task_id", taskId}});
    return m_scheduler->pauseTask(taskId);
}

bool TaskService::resumeTask(const QString& taskId)
{
    if (!m_initialized) {
        Logger::warn("TASK_RESUME_REJECTED", "任务服务未初始化，无法恢复任务", {{"aircraft_task_id", taskId}});
        return false;
    }

    Logger::info("TASK_RESUME_REQUEST", QString("请求恢复飞机任务 %1").arg(taskId), {{"aircraft_task_id", taskId}});
    return m_scheduler->resumeTask(taskId);
}

bool TaskService::cancelTask(const QString& taskId)
{
    if (!m_initialized) {
        Logger::warn("TASK_CANCEL_REJECTED", "任务服务未初始化，无法取消任务", {{"aircraft_task_id", taskId}});
        return false;
    }

    Logger::warn("TASK_CANCEL_REQUEST", QString("请求取消飞机任务 %1").arg(taskId), {{"aircraft_task_id", taskId}});
    return m_scheduler->cancelTask(taskId);
}

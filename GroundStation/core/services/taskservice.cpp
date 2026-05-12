#include "taskservice.h"

#include "../logging/logger.h"
#include "../repository/taskrepository.h"
#include "../domain/download/downloadmanager.h"
#include "../domain/scheduler/taskscheduler.h"
#include "../domain/state/taskstatemachine.h"
#include "../domain/transfer/transfermanager.h"

#include "models.h"

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

QList<AircraftTask> TaskService::getExecutableAircraftTasksForUser(int userId, const QString& role)
{
    if (!m_initialized) {
        return {};
    }

    if (UserRole::roleFromString(role) == UserRole::Operator) {
        return m_repository->getAircraftTasksByAssignedOperator(userId);
    }

    return m_repository->getAllAircraftTasks();
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

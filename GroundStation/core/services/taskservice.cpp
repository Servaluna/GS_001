#include "taskservice.h"

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
        qCritical() << "TaskService::init - invalid dependency";
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
    return true;
}

void TaskService::start()
{
    if (m_initialized) {
        m_scheduler->start();
    }
}

void TaskService::stop()
{
    if (m_initialized) {
        m_scheduler->stop();
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
    return m_initialized && m_scheduler->startTask(taskId);
}

bool TaskService::pauseTask(const QString& taskId)
{
    return m_initialized && m_scheduler->pauseTask(taskId);
}

bool TaskService::resumeTask(const QString& taskId)
{
    return m_initialized && m_scheduler->resumeTask(taskId);
}

bool TaskService::cancelTask(const QString& taskId)
{
    return m_initialized && m_scheduler->cancelTask(taskId);
}

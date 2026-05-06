#ifndef LOCALDAO_H
#define LOCALDAO_H

#include "localmodels/transferringtask.h"

#include <QSqlQuery>

class LocalDAO
{
public:
    explicit LocalDAO();

    // CRUD 操作
    bool insert(const TransferringTask& task);
    bool update(const TransferringTask& task);
    bool remove(int id);
    bool removeByTaskId(const QString& taskId);
    void clearCompleted();        // 清理已完成的任务
    void clearAll();              // 清空所有任务

    // 查询操作
    TransferringTask getTransferringTaskById(int id);
    TransferringTask getTransferringTaskById(const QString& taskId);

    QList<TransferringTask> getAll();
    QList<TransferringTask> getByStatus(TransferStatus ::Status status);
    QList<TransferringTask> getPendingTasks();          // 获取等待执行的任务
    QList<TransferringTask> getRunningTasks();          // 获取执行中的任务

    TransferringTask getCurrentRunningTask();           // 获取当前执行中的任务确切

    //  统计
    int getPendingCount();
    int getRunningCount();
    int getCompletedCount();
    int getFailedCount();

    //  批量操作
    bool updateStatus(const QString& taskId, TransferStatus::Status status);
    bool updateProgress(const QString& taskId, qint64 transferredBytes);
    bool updateCurrentStep(const QString& taskId, CurrentSteps::Steps step);


private:
    TransferringTask rowToTask(const QSqlQuery& query);   // 将查询结果转换为 TransferringTask 对象
};

#endif // LOCALDAO_H

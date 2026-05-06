#ifndef TRANSFERRINGTASK_H
#define TRANSFERRINGTASK_H

#include "../Common/models.h"
#include <QSqlRecord>
#include <QString>

namespace TransferStatus  {
    enum Status {
        Pending,        // 等待执行
        Downloading,    // 下载中
        Uploading,      // 上传中
        Installing,     // 安装中
        Paused,         // 已暂停
        Succeeded,      // 已完成
        Failed,         // 失败
        Cancelled       // 已取消
    };

    inline QString toString(Status status) {
        switch(status) {
        case Pending: return "等待执行";
        case Downloading: return "下载中";
        case Uploading: return "上传中";
        case Installing: return "安装中";
        case Paused: return "已暂停";
        case Succeeded: return "已完成";
        case Failed: return "失败";
        case Cancelled: return "已取消";
        default: return "未知";
        }
    }
}

namespace CurrentSteps {
    enum Steps {
        Idle,
        Preparing,
        Downloading,
        Verifying,
        Sending,
        Installing,
        Cleaning,
        Paused,
        Completed,
        Failed,
        Cancelled
    };
}

class TransferringTask
{
public:
    TransferringTask();
    explicit TransferringTask(const TaskBasicInfo& taskInfo, const FileInfo& fileInfo);

    // 与 transferring_tasks 表字段保持一致。
    QString task_id;
    QString file_id;
    TaskType::Type task_type;
    QString description;
    QString target_device_id;
    int priority;
    QString file_name;
    qint64 file_size;
    QString file_sha256;
    qint64 transferred_bytes;
    TransferStatus::Status status;
    CurrentSteps::Steps current_step;
    QString error_message;
    QDateTime create_time;
    QDateTime start_time;
    QDateTime end_time;
    QDateTime last_update_time;

    QString local_cache_path;
    QString local_temp_path;

    bool isValid() const;
    int getProgressPercent() const;
    QString getStatusText() const;
    bool isRunning() const;
    bool isFinished() const;

    void updateProgress(qint64 bytesTransferred);
    void setStatus(TransferStatus::Status newStatus);

    QJsonObject toJson() const;
    static TransferringTask fromJson(const QJsonObject& json);

    static TransferringTask fromSqlRecord(const QSqlRecord& record);
};

#endif // TRANSFERRINGTASK_H

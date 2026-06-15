#ifndef TASKSTATUS_H
#define TASKSTATUS_H

#include <QString>

enum class BatchTaskStatus : int {
    Created = 0,
    Running = 1,
    Finished = 2,
    Failed = 3,
    Cancelled = 4
};

enum class DeviceTaskStatus : int {
    Waiting = 0,
    Downloading = 1,
    Transferring = 2,
    Installing = 3,
    Verifying = 4,
    Success = 5,
    Failed = 6
};

enum class DownloadSessionStatus : int {
    Pending = 0,
    Downloading = 1,
    Finished = 2,
    Failed = 3,
    Paused = 4,
    Cancelled = 5
};

enum class TransferSessionStatus : int {
    Pending = 0,
    Transferring = 1,
    Finished = 2,
    Failed = 3,
    Paused = 4,
    Cancelled = 5
};

namespace TaskStatusText {

inline int toInt(BatchTaskStatus status)
{
    return static_cast<int>(status);
}

inline int toInt(DeviceTaskStatus status)
{
    return static_cast<int>(status);
}

inline int toInt(DownloadSessionStatus status)
{
    return static_cast<int>(status);
}

inline int toInt(TransferSessionStatus status)
{
    return static_cast<int>(status);
}

inline BatchTaskStatus batchFromInt(int status)
{
    switch (status) {
    case 1: return BatchTaskStatus::Running;
    case 2: return BatchTaskStatus::Finished;
    case 3: return BatchTaskStatus::Failed;
    case 4: return BatchTaskStatus::Cancelled;
    default: return BatchTaskStatus::Created;
    }
}

inline DeviceTaskStatus deviceFromInt(int status)
{
    switch (status) {
    case 1: return DeviceTaskStatus::Downloading;
    case 2: return DeviceTaskStatus::Transferring;
    case 3: return DeviceTaskStatus::Installing;
    case 4: return DeviceTaskStatus::Verifying;
    case 5: return DeviceTaskStatus::Success;
    case 6: return DeviceTaskStatus::Failed;
    default: return DeviceTaskStatus::Waiting;
    }
}

inline DownloadSessionStatus downloadFromInt(int status)
{
    switch (status) {
    case 1: return DownloadSessionStatus::Downloading;
    case 2: return DownloadSessionStatus::Finished;
    case 3: return DownloadSessionStatus::Failed;
    case 4: return DownloadSessionStatus::Paused;
    case 5: return DownloadSessionStatus::Cancelled;
    default: return DownloadSessionStatus::Pending;
    }
}

inline TransferSessionStatus transferFromInt(int status)
{
    switch (status) {
    case 1: return TransferSessionStatus::Transferring;
    case 2: return TransferSessionStatus::Finished;
    case 3: return TransferSessionStatus::Failed;
    case 4: return TransferSessionStatus::Paused;
    case 5: return TransferSessionStatus::Cancelled;
    default: return TransferSessionStatus::Pending;
    }
}

inline bool isFinished(DeviceTaskStatus status)
{
    return status == DeviceTaskStatus::Success || status == DeviceTaskStatus::Failed;
}

inline bool isRunning(DeviceTaskStatus status)
{
    return status == DeviceTaskStatus::Downloading ||
           status == DeviceTaskStatus::Transferring ||
           status == DeviceTaskStatus::Installing ||
           status == DeviceTaskStatus::Verifying;
}

inline QString devicePhase(DeviceTaskStatus status)
{
    switch (status) {
    case DeviceTaskStatus::Waiting: return "waiting";
    case DeviceTaskStatus::Downloading: return "downloading";
    case DeviceTaskStatus::Transferring: return "transferring";
    case DeviceTaskStatus::Installing: return "installing";
    case DeviceTaskStatus::Verifying: return "verifying";
    case DeviceTaskStatus::Success: return "success";
    case DeviceTaskStatus::Failed: return "failed";
    }
    return "waiting";
}

inline QString normalizeDevicePhase(const QString& phase, DeviceTaskStatus fallbackStatus)
{
    const QString normalized = phase.trimmed().toLower();
    if (normalized == "waiting" ||
        normalized == "downloading" ||
        normalized == "transferring" ||
        normalized == "installing" ||
        normalized == "verifying" ||
        normalized == "success" ||
        normalized == "failed") {
        return normalized;
    }

    return devicePhase(fallbackStatus);
}

inline QString devicePhaseDisplayName(const QString& phase, DeviceTaskStatus fallbackStatus)
{
    const QString normalized = normalizeDevicePhase(phase, fallbackStatus);
    if (normalized == "waiting") return "待执行";
    if (normalized == "downloading") return "下载中";
    if (normalized == "transferring") return "传输中";
    if (normalized == "installing") return "安装中";
    if (normalized == "verifying") return "校验中";
    if (normalized == "success") return "已完成";
    if (normalized == "failed") return "失败";
    return "未知";
}

inline QString deviceDisplayName(DeviceTaskStatus status)
{
    switch (status) {
    case DeviceTaskStatus::Waiting: return "待执行";
    case DeviceTaskStatus::Downloading: return "下载中";
    case DeviceTaskStatus::Transferring: return "传输中";
    case DeviceTaskStatus::Installing: return "安装中";
    case DeviceTaskStatus::Verifying: return "校验中";
    case DeviceTaskStatus::Success: return "成功";
    case DeviceTaskStatus::Failed: return "失败";
    }
    return "未知";
}

inline QString batchDisplayName(BatchTaskStatus status)
{
    switch (status) {
    case BatchTaskStatus::Created: return "已创建";
    case BatchTaskStatus::Running: return "运行中";
    case BatchTaskStatus::Finished: return "已结束";
    case BatchTaskStatus::Failed: return "失败";
    case BatchTaskStatus::Cancelled: return "已取消";
    }
    return "未知";
}

}

#endif // TASKSTATUS_H

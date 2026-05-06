#ifndef INSTALLCONTEXT_H
#define INSTALLCONTEXT_H

#include <QString>
#include <QStringList>
#include <QFile>

struct InstallContext
{
    QString taskId;           // 任务ID
    QString targetDeviceId;   // 目标设备ID
    QString deviceType;       // 设备类型（用于选择安装策略）
    QString filePath;         // 待安装文件的本地路径
    QString fileSha256;       // 文件 SHA-256
    QString fileName;         // 原始文件名
    qint64 fileSize;          // 文件大小

    // 安装路径配置（可从配置文件读取）
    QString targetPath;       // 目标安装路径
    QString backupPath;       // 备份路径
    QStringList preInstallScript;   // 安装前执行的脚本
    QStringList postInstallScript;  // 安装后执行的脚本
    int timeoutMs;            // 超时时间

    // 版本信息
    QString currentVersion;   // 当前版本
    QString targetVersion;    // 目标版本

    InstallContext() : fileSize(0), timeoutMs(60000) {}

    bool isValid() const
    {
        return !taskId.isEmpty()
        && !targetDeviceId.isEmpty()
            && !filePath.isEmpty()
            && QFile::exists(filePath);
    }
};

#endif // INSTALLCONTEXT_H

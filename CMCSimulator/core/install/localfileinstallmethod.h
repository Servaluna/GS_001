#ifndef LOCALFILEINSTALLMETHOD_H
#define LOCALFILEINSTALLMETHOD_H

#include <QObject>

#include "IInstallMethod.h"
#include <QMap>
#include <QTimer>
#include <QProcess>
#include <QDateTime>

class LocalFileInstallMethod : public IInstallMethod
{
    Q_OBJECT
public:
    explicit LocalFileInstallMethod(QObject *parent = nullptr);
    ~LocalFileInstallMethod();

    void startInstall(const InstallContext& context) override;
    void cancelInstall(const QString& taskId) override;
    bool isInstalling(const QString& taskId) const override;
    QString methodName() const override { return "LocalFileInstallMethod"; }

private:
    //安装步骤枚举

        enum class InstallStep {
            Backup = 0,      // 备份原文件
            CopyFile,        // 复制文件
            VerifyCopy,      // 验证复制
            PreScript,       // 执行安装前脚本
            PostScript,      // 执行安装后脚本
            VerifyInstall,   // 验证安装
            Completed        // 完成
        };

        InstallStep stepFromString(const QString& stepStr) const
        {
            if(stepStr == "Backup") return InstallStep::Backup;
            if(stepStr == "CopyFile") return InstallStep::CopyFile;
            if(stepStr == "VerifyCopy") return InstallStep::VerifyCopy;
            if(stepStr == "PreScript") return InstallStep::PreScript;
            if(stepStr == "PostScript") return InstallStep::PostScript;
            if(stepStr == "VerifyInstall") return InstallStep::VerifyInstall;
            if(stepStr == "Completed") return InstallStep::Completed;
            return InstallStep::Backup;
        };
        QString stepToString(InstallStep step) const
        {
            switch (step) {
            case InstallStep::Backup: return "Backup";
            case InstallStep::CopyFile: return "CopyFile";
            case InstallStep::VerifyCopy: return "VerifyCopy";
            case InstallStep::PreScript: return "PreScript";
            case InstallStep::PostScript: return "PostScript";
            case InstallStep::VerifyInstall: return "VerifyInstall";
            case InstallStep::Completed: return "Completed";
            default: return "Unknown";
            }
        }

    //安装任务信息
    struct InstallTask {
        InstallContext context;
        QTimer* timeoutTimer;
        QProcess* scriptProcess;
        bool isCancelled;
        QString backupFilePath;
        int currentStep;  // 0=backup,1=copy,2=preScript,3=postScript,4=verify
        QDateTime startTime;

        InstallTask() : timeoutTimer(nullptr), scriptProcess(nullptr),
            isCancelled(false), currentStep(0) {}

        ~InstallTask()
        {
            if (timeoutTimer) {
                timeoutTimer->stop();
                delete timeoutTimer;
            }
            if (scriptProcess) {
                scriptProcess->kill();
                scriptProcess->deleteLater();
            }
        }
    };

    // 安装步骤（异步链式调用）
    void doBackup(InstallTask* task);
    void doCopyFile(InstallTask* task);
    void doVerifyCopy(InstallTask* task);
    void doPreInstallScript(InstallTask* task);
    void doPostInstallScript(InstallTask* task);
    void doVerifyInstallation(InstallTask* task);
    void finishInstall(InstallTask* task, bool success, const QString& message, const QString& newVersion);

    // 回滚
    void rollback(InstallTask* task, const QString& reason);
    bool restoreBackup(InstallTask* task);

    // 辅助方法
    bool createBackup(InstallTask* task);
    bool copyFileWithVerify(const QString& src, const QString& dst, const QString& expectedSha256);
    QString calculateFileSha256(const QString& filePath);
    bool runScript(const QString& scriptPath, int timeoutMs, QString& output);
    QString getDeviceVersion(const QString& deviceId);
    void ensureDirectoryExists(const QString& path);


    // 超时处理
    void onTimeout(InstallTask* task);
    void onScriptFinished(InstallTask* task, int exitCode, QProcess::ExitStatus exitStatus);


    InstallStep m_currentStep = InstallStep::Backup;
    // 任务管理
    QMap<QString, InstallTask*> m_activeTasks;

    // 配置
    static constexpr int DEFAULT_TIMEOUT_MS = 60000;
    static constexpr int SCRIPT_TIMEOUT_MS = 30000;

};

#endif // LOCALFILEINSTALLMETHOD_H

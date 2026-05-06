#include "localfileinstallmethod.h"

#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>

LocalFileInstallMethod::LocalFileInstallMethod(QObject *parent)
    : IInstallMethod{parent}
{}

LocalFileInstallMethod::~LocalFileInstallMethod()
{
    for (auto task : m_activeTasks.values()) {
        delete task;
    }
    m_activeTasks.clear();
}

void LocalFileInstallMethod::startInstall(const InstallContext& context)
{
    if (!context.isValid()) {
        qWarning() << "LocalFileInstallMethod: Invalid install context";
        emit installFinished(context.taskId, false, "安装上下文无效", QString());
        return;
    }

    if (m_activeTasks.contains(context.taskId)) {
        qWarning() << "LocalFileInstallMethod: Task already running:" << context.taskId;
        emit installFinished(context.taskId, false, "任务已在执行中", QString());
        return;
    }

    qDebug() << "LocalFileInstallMethod: Starting install for task:" << context.taskId
             << "device:" << context.targetDeviceId
             << "file:" << context.filePath;

    InstallTask* task = new InstallTask();
    task->context = context;
    task->startTime = QDateTime::currentDateTime();

    // 设置超时定时器
    task->timeoutTimer = new QTimer(this);
    task->timeoutTimer->setSingleShot(true);
    int timeout = context.timeoutMs > 0 ? context.timeoutMs : DEFAULT_TIMEOUT_MS;
    task->timeoutTimer->start(timeout);
    connect(task->timeoutTimer, &QTimer::timeout, this, [this, task]() { onTimeout(task); });

    m_activeTasks[context.taskId] = task;

    // 开始第一步：备份
    doBackup(task);
}

void LocalFileInstallMethod::cancelInstall(const QString& taskId)
{
    auto it = m_activeTasks.find(taskId);
    if (it == m_activeTasks.end()) {
        qWarning() << "LocalFileInstallMethod: Task not found for cancel:" << taskId;
        return;
    }

    InstallTask* task = it.value();
    task->isCancelled = true;

    qDebug() << "LocalFileInstallMethod: Cancelling task:" << taskId;

    if (task->scriptProcess && task->scriptProcess->state() == QProcess::Running) {
        task->scriptProcess->kill();
    }

    rollback(task, "用户取消");
}

bool LocalFileInstallMethod::isInstalling(const QString& taskId) const
{
    return m_activeTasks.contains(taskId);
}

// ==================== 安装步骤 ====================

void LocalFileInstallMethod::doBackup(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    task->currentStep = static_cast<int>(InstallStep::Backup);
    emit progressUpdated(task->context.taskId, 5, "正在备份原文件");

    if (!createBackup(task)) {
        rollback(task, "备份原文件失败");
        return;
    }

    // 进入下一步
    QTimer::singleShot(0, this, [this, task]() { doCopyFile(task); });
}

void LocalFileInstallMethod::doCopyFile(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    task->currentStep =static_cast<int> (InstallStep::CopyFile);
    emit progressUpdated(task->context.taskId, 20, "正在复制文件");

    QString targetFilePath = task->context.targetPath + "/" + task->context.fileName;
    ensureDirectoryExists(task->context.targetPath);

    if (!QFile::copy(task->context.filePath, targetFilePath)) {
        qWarning() << "LocalFileInstallMethod: Failed to copy file to" << targetFilePath;
        rollback(task, "复制文件失败");
        return;
    }

    qDebug() << "LocalFileInstallMethod: File copied to" << targetFilePath;
    QTimer::singleShot(0, this, [this, task]() { doVerifyCopy(task); });
}

void LocalFileInstallMethod::doVerifyCopy(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    task->currentStep = static_cast<int>(InstallStep::VerifyCopy);
    emit progressUpdated(task->context.taskId, 40, "正在验证文件");

    QString targetFilePath = task->context.targetPath + "/" + task->context.fileName;

    if (!copyFileWithVerify(task->context.filePath, targetFilePath, task->context.fileSha256)) {
        rollback(task, "文件复制验证失败");
        return;
    }

    qDebug() << "LocalFileInstallMethod: File verification passed";
    QTimer::singleShot(0, this, [this, task]() { doPreInstallScript(task); });
}

void LocalFileInstallMethod::doPreInstallScript(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    if (task->context.preInstallScript.isEmpty()) {
        // 没有预安装脚本，直接进入下一步
        QTimer::singleShot(0, this, [this, task]() { doPostInstallScript(task); });
        return;
    }

    task->currentStep = static_cast<int> (InstallStep::PreScript);
    emit progressUpdated(task->context.taskId, 60, "正在执行安装前脚本");

    // 执行脚本（简化：只执行第一个脚本）
    QString scriptPath = task->context.preInstallScript.first();
    QString output;

    if (!runScript(scriptPath, SCRIPT_TIMEOUT_MS, output)) {
        qWarning() << "LocalFileInstallMethod: Pre-install script failed:" << output;
        rollback(task, "安装前脚本执行失败: " + output);
        return;
    }

    qDebug() << "LocalFileInstallMethod: Pre-install script completed";
    QTimer::singleShot(0, this, [this, task]() { doPostInstallScript(task); });
}

void LocalFileInstallMethod::doPostInstallScript(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    if (task->context.postInstallScript.isEmpty()) {
        // 没有后安装脚本，直接进入验证
        QTimer::singleShot(0, this, [this, task]() { doVerifyInstallation(task); });
        return;
    }

    task->currentStep = static_cast<int> (InstallStep::PostScript);
    emit progressUpdated(task->context.taskId, 80, "正在执行安装后脚本");

    QString scriptPath = task->context.postInstallScript.first();
    QString output;

    if (!runScript(scriptPath, SCRIPT_TIMEOUT_MS, output)) {
        qWarning() << "LocalFileInstallMethod: Post-install script failed:" << output;
        rollback(task, "安装后脚本执行失败: " + output);
        return;
    }

    qDebug() << "LocalFileInstallMethod: Post-install script completed";
    QTimer::singleShot(0, this, [this, task]() { doVerifyInstallation(task); });
}

void LocalFileInstallMethod::doVerifyInstallation(InstallTask* task)
{
    if (task->isCancelled) {
        rollback(task, "任务已取消");
        return;
    }

    task->currentStep = static_cast<int> (InstallStep::VerifyInstall);
    emit progressUpdated(task->context.taskId, 90, "正在验证安装");

    // 验证：检查目标文件是否存在且 SHA-256 正确
    QString targetFilePath = task->context.targetPath + "/" + task->context.fileName;
    if (!QFile::exists(targetFilePath)) {
        rollback(task, "安装验证失败：目标文件不存在");
        return;
    }

    QString actualSha256 = calculateFileSha256(targetFilePath);
    if (actualSha256.compare(task->context.fileSha256, Qt::CaseInsensitive) != 0) {
        rollback(task, "安装验证失败：文件 SHA-256 不匹配");
        return;
    }

    // 获取新版本号（如果有）
    QString newVersion = getDeviceVersion(task->context.targetDeviceId);

    int elapsedMs = task->startTime.msecsTo(QDateTime::currentDateTime());
    qDebug() << "LocalFileInstallMethod: Installation verified successfully, elapsed:" << elapsedMs << "ms";

    finishInstall(task, true, "安装成功", newVersion);
}

void LocalFileInstallMethod::finishInstall(InstallTask* task, bool success,
                                           const QString& message, const QString& newVersion)
{
    if (task->timeoutTimer) {
        task->timeoutTimer->stop();
    }

    emit progressUpdated(task->context.taskId, 100, success ? "安装完成" : "安装失败");
    emit installFinished(task->context.taskId, success, message, newVersion);

    qDebug() << "LocalFileInstallMethod: Install finished - task:" << task->context.taskId
             << "success:" << success << "message:" << message;

    m_activeTasks.remove(task->context.taskId);
    delete task;
}

// ==================== 回滚机制 ====================

void LocalFileInstallMethod::rollback(InstallTask* task, const QString& reason)
{
    qWarning() << "LocalFileInstallMethod: Rolling back installation for task:"
               << task->context.taskId << "reason:" << reason;

    emit progressUpdated(task->context.taskId, 95, "正在回滚...");

    bool rollbackSuccess = restoreBackup(task);

    if (rollbackSuccess) {
        finishInstall(task, false, reason + "（已回滚）", QString());
    } else {
        finishInstall(task, false, reason + "（回滚失败，请手动恢复）", QString());
    }
}

bool LocalFileInstallMethod::restoreBackup(InstallTask* task)
{
    if (task->backupFilePath.isEmpty()) {
        qDebug() << "LocalFileInstallMethod: No backup to restore";
        return true;
    }

    QString targetFilePath = task->context.targetPath + "/" + task->context.fileName;

    if (!QFile::exists(task->backupFilePath)) {
        qWarning() << "LocalFileInstallMethod: Backup file not found:" << task->backupFilePath;
        return false;
    }

    // 删除损坏的文件
    if (QFile::exists(targetFilePath)) {
        QFile::remove(targetFilePath);
    }

    // 恢复备份
    if (!QFile::copy(task->backupFilePath, targetFilePath)) {
        qWarning() << "LocalFileInstallMethod: Failed to restore backup";
        return false;
    }

    qDebug() << "LocalFileInstallMethod: Backup restored successfully";
    return true;
}

// ==================== 辅助方法 ====================

bool LocalFileInstallMethod::createBackup(InstallTask* task)
{
    QString targetFilePath = task->context.targetPath + "/" + task->context.fileName;

    // 如果目标文件不存在，不需要备份
    if (!QFile::exists(targetFilePath)) {
        qDebug() << "LocalFileInstallMethod: Target file doesn't exist, no backup needed";
        return true;
    }

    // 确保备份目录存在
    ensureDirectoryExists(task->context.backupPath);

    // 生成备份文件名（包含时间戳）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFileName = task->context.fileName + "." + timestamp + ".bak";
    task->backupFilePath = task->context.backupPath + "/" + backupFileName;

    if (!QFile::copy(targetFilePath, task->backupFilePath)) {
        qWarning() << "LocalFileInstallMethod: Failed to create backup:" << task->backupFilePath;
        return false;
    }

    qDebug() << "LocalFileInstallMethod: Backup created at" << task->backupFilePath;
    return true;
}

bool LocalFileInstallMethod::copyFileWithVerify(const QString& src, const QString& dst,
                                                const QString& expectedSha256)
{
    // 复制文件
    if (!QFile::copy(src, dst)) {
        qWarning() << "LocalFileInstallMethod: Failed to copy file from" << src << "to" << dst;
        return false;
    }

    QString actualSha256 = calculateFileSha256(dst);
    if (actualSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        qWarning() << "LocalFileInstallMethod: SHA-256 mismatch - expected:" << expectedSha256
                   << "actual:" << actualSha256;
        return false;
    }

    return true;
}

QString LocalFileInstallMethod::calculateFileSha256(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    return QString();
}

bool LocalFileInstallMethod::runScript(const QString& scriptPath, int timeoutMs, QString& output)
{
    if (!QFile::exists(scriptPath)) {
        output = "脚本不存在: " + scriptPath;
        return false;
    }

    QProcess process;
    process.start(scriptPath);

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        output = "脚本执行超时";
        return false;
    }

    if (process.exitCode() != 0) {
        output = QString::fromUtf8(process.readAllStandardError());
        return false;
    }

    output = QString::fromUtf8(process.readAllStandardOutput());
    return true;
}

QString LocalFileInstallMethod::getDeviceVersion(const QString& deviceId)
{
    // 简化实现：可以从版本文件读取
    // 实际实现可能从配置文件或设备状态中获取
    Q_UNUSED(deviceId);
    return "1.0.0";
}

void LocalFileInstallMethod::ensureDirectoryExists(const QString& path)
{
    QDir dir;
    if (!dir.exists(path)) {
        dir.mkpath(path);
    }
}

void LocalFileInstallMethod::onTimeout(InstallTask* task)
{
    qWarning() << "LocalFileInstallMethod: Timeout for task:" << task->context.taskId;
    rollback(task, "安装超时");
}

void LocalFileInstallMethod::onScriptFinished(InstallTask* task, int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    // 根据当前步骤决定下一步
    switch (task->currentStep) {
    case static_cast<int>(InstallStep::PreScript):
        QTimer::singleShot(0, this, [this, task]() { doPostInstallScript(task); });
        break;
    case static_cast<int>(InstallStep::PostScript):
        QTimer::singleShot(0, this, [this, task]() { doVerifyInstallation(task); });
        break;
    default:
        break;
    }
}

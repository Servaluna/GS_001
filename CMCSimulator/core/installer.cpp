#include "installer.h"

#include "install/LocalFileInstallMethod.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
// #include <QJsonObject>

Installer::Installer(QObject *parent)
    : QObject{parent}
    , m_defaultMethod(nullptr)
{
    // 初始化配置文件搜索路径
    m_configSearchPaths << QCoreApplication::applicationDirPath() + "/config/install_config.json"
                        << QCoreApplication::applicationDirPath() + "/install_config.json"
                        << "/etc/cmcsimulator/install_config.json"
                        << ":/config/install_config.json";
}

Installer::~Installer()
{
    for (auto method : m_methods.values()) {
        method->deleteLater();
    }
    if (m_defaultMethod) {
        m_defaultMethod->deleteLater();
    }
}

bool Installer::init(const QString& configPath)
{
    // 1. 注册默认的本地文件安装方法
    auto* localMethod = new LocalFileInstallMethod(this);
    registerInstallMethod("default", localMethod);
    m_defaultMethod = localMethod;

    // 2. 加载配置文件
    bool configLoaded = false;

    // 如果指定了配置文件路径，优先使用
    if (!configPath.isEmpty() && QFile::exists(configPath)) {
        configLoaded = loadConfig(configPath);
    }

    // 否则搜索默认路径
    if (!configLoaded) {
        for (const QString& path : m_configSearchPaths) {
            if (QFile::exists(path)) {
                configLoaded = loadConfig(path);
                if (configLoaded) {
                    qDebug() << "Installer: Loaded config from" << path;
                    break;
                }
            }
        }
    }

    // 如果配置文件加载失败，使用默认配置
    if (!configLoaded) {
        qWarning() << "Installer: No config file found, using defaults";
        useDefaultConfig();
    }

    qDebug() << "Installer: Initialized with" << m_methods.size() << "install methods"
             << "and" << m_deviceConfigs.size() << "device configs";
    return true;
}

void Installer::install(const QString& taskId,
                        const QString& targetDeviceId,
                        const QString& filePath)
{
    qDebug() << "Installer: Installing task" << taskId << "to device" << targetDeviceId;

    // 检查是否已经在安装
    if (m_taskToDevice.contains(taskId)) {
        qWarning() << "Installer: Task already in progress:" << taskId;
        emit installFinished(taskId, false, "任务已在执行中");
        return;
    }

    // 构建安装上下文
    InstallContext context = buildContext(taskId, targetDeviceId, filePath);
    if (!context.isValid()) {
        qWarning() << "Installer: Invalid install context for task:" << taskId;
        emit installFinished(taskId, false, "安装上下文无效");
        return;
    }

    // 获取安装方法
    IInstallMethod* method = getInstallMethod(targetDeviceId);
    if (!method) {
        qWarning() << "Installer: No install method for device:" << targetDeviceId;
        emit installFinished(taskId, false, "未找到合适的安装方法");
        return;
    }

    // 连接信号
    connect(method, &IInstallMethod::progressUpdated,
            this, &Installer::onProgressUpdated, Qt::UniqueConnection);
    connect(method, &IInstallMethod::installFinished,
            this, &Installer::onMethodFinished, Qt::UniqueConnection);

    // 记录任务
    m_taskToDevice[taskId] = targetDeviceId;

    // 开始安装
    method->startInstall(context);
}

void Installer::cancelInstall(const QString& taskId)
{
    QString deviceId = m_taskToDevice.value(taskId);
    if (deviceId.isEmpty()) {
        qWarning() << "Installer: Task not found for cancel:" << taskId;
        return;
    }

    IInstallMethod* method = getInstallMethod(deviceId);
    if (method) {
        method->cancelInstall(taskId);
    }

    m_taskToDevice.remove(taskId);
}

bool Installer::isInstalling(const QString& taskId) const
{
    return m_taskToDevice.contains(taskId);
}

void Installer::registerInstallMethod(const QString& deviceType, IInstallMethod* method)
{
    if (m_methods.contains(deviceType)) {
        qWarning() << "Installer: Method already registered for type:" << deviceType;
        return;
    }
    m_methods[deviceType] = method;
    qDebug() << "Installer: Registered install method for type:" << deviceType;
}

// ==================== 私有槽函数 ====================

void Installer::onProgressUpdated(QString taskId, int percent, QString step)
{
    emit installProgress(taskId, percent, step);
}

void Installer::onMethodFinished(QString taskId, bool success, QString message, QString newVersion)
{
    Q_UNUSED(newVersion);

    qDebug() << "Installer: Task finished - taskId:" << taskId
             << "success:" << success
             << "message:" << message;

    m_taskToDevice.remove(taskId);
    emit installFinished(taskId, success, message);
}

// ==================== 私有方法 ====================

IInstallMethod* Installer::getInstallMethod(const QString& deviceId)
{
    // 根据设备ID获取设备类型
    DeviceConfig config = getDeviceConfig(deviceId);
    QString deviceType = config.deviceType;

    if (deviceType.isEmpty() || !m_methods.contains(deviceType)) {
        qDebug() << "Installer: Using default method for device:" << deviceId;
        return m_defaultMethod;
    }

    return m_methods[deviceType];
}

InstallContext Installer::buildContext(const QString& taskId,
                                       const QString& targetDeviceId,
                                       const QString& filePath)
{
    InstallContext context;
    context.taskId = taskId;
    context.targetDeviceId = targetDeviceId;
    context.filePath = filePath;

    // 获取文件信息
    QFileInfo fileInfo(filePath);
    context.fileName = fileInfo.fileName();
    context.fileSize = fileInfo.size();

    // 获取设备配置
    DeviceConfig config = getDeviceConfig(targetDeviceId);
    context.deviceType = config.deviceType;
    context.targetPath = config.targetPath;
    context.backupPath = config.backupPath;
    context.preInstallScript = config.preInstallScript;
    context.postInstallScript = config.postInstallScript;
    context.timeoutMs = config.timeoutMs;

    // TODO: 计算文件 SHA-256（可能需要从外部传入，避免重复计算）
    // 这里简化处理，实际应该从 FileReceiver 传入

    return context;
}

bool Installer::loadConfig(const QString& configPath)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Installer: Cannot open config file:" << configPath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Installer: Invalid JSON config file:" << configPath;
        return false;
    }

    return loadConfigFromJson(doc.object());
}

bool Installer::loadConfigFromJson(const QJsonObject& json)
{
    // 加载默认配置
    if (json.contains("default") && json["default"].isObject()) {
        QJsonObject defaultObj = json["default"].toObject();
        DeviceConfig defaultConfig;
        defaultConfig.deviceType = "default";
        defaultConfig.targetPath = defaultObj.value("target_path").toString("/opt/devices/default/");
        defaultConfig.backupPath = defaultObj.value("backup_path").toString("/opt/backups/");
        defaultConfig.timeoutMs = defaultObj.value("timeout_ms").toInt(60000);

        // 读取脚本列表
        if (defaultObj.contains("pre_install_script") && defaultObj["pre_install_script"].isArray()) {
            QJsonArray scripts = defaultObj["pre_install_script"].toArray();
            for (const auto& script : scripts) {
                defaultConfig.preInstallScript << script.toString();
            }
        }
        if (defaultObj.contains("post_install_script") && defaultObj["post_install_script"].isArray()) {
            QJsonArray scripts = defaultObj["post_install_script"].toArray();
            for (const auto& script : scripts) {
                defaultConfig.postInstallScript << script.toString();
            }
        }

        // 存储默认配置（使用特殊 key）
        m_deviceConfigs["__default__"] = defaultConfig;
    }

    // 加载设备配置
    if (json.contains("devices") && json["devices"].isObject()) {
        QJsonObject devicesObj = json["devices"].toObject();
        for (auto it = devicesObj.begin(); it != devicesObj.end(); ++it) {
            QString deviceId = it.key();
            QJsonObject deviceObj = it.value().toObject();

            DeviceConfig config;
            config.deviceType = deviceObj.value("device_type").toString("default");
            config.targetPath = deviceObj.value("target_path").toString();
            config.backupPath = deviceObj.value("backup_path").toString();
            config.timeoutMs = deviceObj.value("timeout_ms").toInt(60000);

            // 读取脚本列表
            if (deviceObj.contains("pre_install_script") && deviceObj["pre_install_script"].isArray()) {
                QJsonArray scripts = deviceObj["pre_install_script"].toArray();
                for (const auto& script : scripts) {
                    config.preInstallScript << script.toString();
                }
            }
            if (deviceObj.contains("post_install_script") && deviceObj["post_install_script"].isArray()) {
                QJsonArray scripts = deviceObj["post_install_script"].toArray();
                for (const auto& script : scripts) {
                    config.postInstallScript << script.toString();
                }
            }

            m_deviceConfigs[deviceId] = config;
            qDebug() << "Installer: Loaded config for device:" << deviceId
                     << "type:" << config.deviceType;
        }
    }

    return true;
}

void Installer::useDefaultConfig()
{
    // 默认设备配置
    DeviceConfig defaultConfig;
    defaultConfig.deviceType = "default";
    defaultConfig.targetPath = "./test_output/";
    defaultConfig.backupPath = "./test_backups/";
    defaultConfig.timeoutMs = 60000;
    m_deviceConfigs["__default__"] = defaultConfig;

    // 模拟设备配置
    DeviceConfig deviceA;
    deviceA.deviceType = "default";
    deviceA.targetPath = "./test_output/device-a/";
    deviceA.backupPath = "./test_backups/device-a/";
    deviceA.timeoutMs = 120000;
    m_deviceConfigs["device-A"] = deviceA;

    DeviceConfig deviceB;
    deviceB.deviceType = "default";
    deviceB.targetPath = "./test_output/device-b/";
    deviceB.backupPath = "./test_backups/device-b/";
    deviceB.timeoutMs = 30000;
    m_deviceConfigs["device-B"] = deviceB;

    qDebug() << "Installer: Using default device configurations";
}

Installer::DeviceConfig Installer::getDeviceConfig(const QString& deviceId) const
{
    auto it = m_deviceConfigs.find(deviceId);
    if (it != m_deviceConfigs.end()) {
        return it.value();
    }

    // 返回默认配置
    auto defaultIt = m_deviceConfigs.find("__default__");
    if (defaultIt != m_deviceConfigs.end()) {
        return defaultIt.value();
    }

    // 返回空配置（不应该发生）
    DeviceConfig emptyConfig;
    emptyConfig.deviceType = "default";
    return emptyConfig;
}

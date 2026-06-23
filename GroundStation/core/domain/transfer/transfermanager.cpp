#include "transfermanager.h"

#include "../../logging/logger.h"
#include "../../network/deviceconnector.h"

TransferManager::TransferManager(QObject *parent)
    : QObject{parent}
    , m_deviceConnector(nullptr)
{}

bool TransferManager::init(DeviceConnector* deviceConnector)
{
    if (!deviceConnector) {
        Logger::error("TRANSFER_MANAGER_INIT_FAILED", "TransferManager 初始化失败，DeviceConnector 为空");
        return false;
    }

    m_deviceConnector = deviceConnector;
    connect(m_deviceConnector, &DeviceConnector::sendFinished,
            this, &TransferManager::sendFinished);
    connect(m_deviceConnector, &DeviceConnector::sendProgress,
            this, &TransferManager::transferProgress);
    connect(m_deviceConnector, &DeviceConnector::installResult,
            this, &TransferManager::installResult);
    return true;
}

bool TransferManager::isDeviceOnline(const QString& deviceId) const
{
    return m_deviceConnector && m_deviceConnector->isDeviceOnline(deviceId);
}

void TransferManager::sendFileToDevice(const QString& taskId,
                                       const QString& targetDeviceId,
                                       const QString& localPath,
                                       const QString& fileName,
                                       const QString& sha256)
{
    if (!m_deviceConnector) {
        Logger::error("TRANSFER_START_FAILED",
                      "设备连接器未初始化，无法传输文件",
                      {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}, {"file_name", fileName}});
        emit sendFinished(taskId, false, "设备连接器未初始化");
        return;
    }

    Logger::info("TRANSFER_START",
                 QString("开始向设备 %1 传输文件").arg(targetDeviceId),
                 {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}, {"file_name", fileName}});
    m_deviceConnector->sendFileToDevice(taskId, targetDeviceId, localPath, fileName, sha256);
}

bool TransferManager::requestBatchInstall(const QString& aircraftTaskId)
{
    if (!m_deviceConnector) {
        Logger::error("INSTALL_START_FAILED",
                      "设备连接器未初始化，无法启动统一安装",
                      {{"aircraft_task_id", aircraftTaskId}});
        return false;
    }

    return m_deviceConnector->requestBatchInstall(aircraftTaskId);
}

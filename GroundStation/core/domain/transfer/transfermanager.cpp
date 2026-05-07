#include "transfermanager.h"

#include "../../network/deviceconnector.h"

TransferManager::TransferManager(QObject *parent)
    : QObject{parent}
    , m_deviceConnector(nullptr)
{}

bool TransferManager::init(DeviceConnector* deviceConnector)
{
    if (!deviceConnector) {
        qCritical() << "TransferManager::init - invalid DeviceConnector";
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
        emit sendFinished(taskId, false, "设备连接器未初始化");
        return;
    }

    m_deviceConnector->sendFileToDevice(taskId, targetDeviceId, localPath, fileName, sha256);
}

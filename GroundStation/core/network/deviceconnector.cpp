#include "deviceconnector.h"

#include "../logging/logger.h"

#include <QDataStream>
#include <QDateTime>
#include <QHostAddress>
#include <QTimer>

DeviceConnector::DeviceConnector(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_socket(nullptr)
    , m_cmcPort(0)
    , m_isConnected(false)
    , m_currentFile(nullptr)
    , m_sendTimer(new QTimer(this))
    , m_fileSize(0)
    , m_sentBytes(0)
{
    connect(m_server, &QTcpServer::newConnection,
            this, &DeviceConnector::onNewConnection);
    m_sendTimer->setSingleShot(true);
    connect(m_sendTimer, &QTimer::timeout,
            this, &DeviceConnector::onSendFileData);
}

DeviceConnector::~DeviceConnector()
{
    cleanupFileTransfer();
    cleanupClientConnection();
    if (m_server->isListening()) {
        m_server->close();
    }
}

bool DeviceConnector::startListening(quint16 port)
{
    const quint16 listenPort = port == 0 ? DEFAULT_LISTEN_PORT : port;
    if (m_server->isListening()) {
        return true;
    }

    if (!m_server->listen(QHostAddress::Any, listenPort)) {
        const QString error = m_server->errorString();
        Logger::error("AIRCRAFT_LISTEN_FAILED",
                      "地面站监听飞机连接失败",
                      {{"port", listenPort}, {"error", error}});
        emit cmcConnectionChanged(false, error);
        return false;
    }

    m_cmcPort = listenPort;
    Logger::info("AIRCRAFT_LISTEN_STARTED",
                 QString("地面站开始监听飞机连接，端口: %1").arg(listenPort),
                 {{"port", listenPort}});
    return true;
}

void DeviceConnector::stopListening()
{
    cleanupClientConnection();
    if (m_server->isListening()) {
        m_server->close();
    }
    Logger::info("AIRCRAFT_LISTEN_STOPPED", "地面站已停止监听飞机连接");
}

bool DeviceConnector::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState && m_isConnected;
}

QString DeviceConnector::connectedAircraftCode() const
{
    return m_aircraftCode;
}

void DeviceConnector::sendFileToDevice(const QString& taskId,
                                       const QString& targetDeviceId,
                                       const QString& localPath,
                                       const QString& fileName,
                                       const QString& sha256)
{
    if (!isConnected()) {
        Logger::warn("TRANSFER_START_FAILED",
                     "未连接飞机，无法发送文件",
                     __FILE__, Q_FUNC_INFO,
                     {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}});
        emit sendFinished(taskId, false, "未连接飞机");
        return;
    }

    if (!isDeviceOnline(targetDeviceId)) {
        Logger::warn("TRANSFER_START_FAILED",
                     "目标设备不在线",
                     __FILE__, Q_FUNC_INFO,
                     {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}});
        emit sendFinished(taskId, false, QString("目标设备 %1 不在线").arg(targetDeviceId));
        return;
    }

    QFile* file = new QFile(localPath, this);
    if (!file->exists()) {
        Logger::error("TRANSFER_START_FAILED",
                      "本地文件不存在",
                      __FILE__, Q_FUNC_INFO,
                      {{"device_task_id", taskId}, {"local_path", localPath}});
        emit sendFinished(taskId, false, "本地文件不存在");
        delete file;
        return;
    }

    if (!file->open(QIODevice::ReadOnly)) {
        Logger::error("TRANSFER_START_FAILED",
                      "无法打开本地文件",
                      __FILE__, Q_FUNC_INFO,
                      {{"device_task_id", taskId}, {"local_path", localPath}});
        emit sendFinished(taskId, false, "无法打开本地文件");
        delete file;
        return;
    }

    cleanupFileTransfer();

    m_currentTaskId = taskId;
    m_currentTargetDeviceId = targetDeviceId;
    m_currentFile = file;
    m_fileSize = file->size();
    m_sentBytes = 0;
    m_fileSha256 = sha256;

    Logger::info("TRANSFER_FILE_OPENED",
                 "本地文件已打开，准备发送文件开始命令",
                 __FILE__, Q_FUNC_INFO,
                 {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}, {"file_name", fileName}, {"file_size", m_fileSize}, {"sha256", sha256}});

    if (!sendFileStart(taskId, targetDeviceId, fileName, m_fileSize, sha256)) {
        Logger::error("TRANSFER_START_FAILED",
                      "发送文件开始命令失败",
                      __FILE__, Q_FUNC_INFO,
                      {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}});
        emit sendFinished(taskId, false, "发送文件开始命令失败");
        cleanupFileTransfer();
        return;
    }

    if (m_sendTimer) {
        m_sendTimer->start(SEND_INTERVAL_MS);
    }
}

DeviceStatus DeviceConnector::getDeviceStatus(const QString& deviceId) const
{
    return m_deviceStatusMap.value(deviceId);
}

QList<DeviceStatus> DeviceConnector::getAllDeviceStatus() const
{
    return m_deviceStatusMap.values();
}

bool DeviceConnector::isDeviceOnline(const QString& deviceId) const
{
    auto it = m_deviceStatusMap.find(deviceId);
    return it != m_deviceStatusMap.end() && it.value().isOnline;
}

bool DeviceConnector::requestBatchInstall(const QString& aircraftTaskId)
{
    if (!isConnected()) {
        Logger::warn("INSTALL_START_FAILED",
                     "未连接飞机，无法启动统一安装",
                     __FILE__, Q_FUNC_INFO,
                     {{"aircraft_task_id", aircraftTaskId}});
        return false;
    }

    return sendInstallStart(aircraftTaskId);
}

void DeviceConnector::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        if (isConnected()) {
            Logger::warn("AIRCRAFT_CONNECTION_REJECTED",
                         "已有飞机连接，拒绝新的飞机连接",
                         {{"ip", socket->peerAddress().toString()}, {"port", socket->peerPort()}});
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        m_socket = socket;
        m_cmcIp = socket->peerAddress().toString();
        m_cmcPort = socket->peerPort();
        m_isConnected = true;
        m_aircraftCode.clear();
        m_receiveBuffer.clear();

        connect(m_socket, &QTcpSocket::disconnected,
                this, &DeviceConnector::onDisconnected);
        connect(m_socket, &QTcpSocket::errorOccurred,
                this, &DeviceConnector::onErrorOccurred);
        connect(m_socket, &QTcpSocket::readyRead,
                this, &DeviceConnector::onReadyRead);

        Logger::info("AIRCRAFT_CONNECTED",
                     QString("飞机已连接到地面站 %1:%2").arg(m_cmcIp).arg(m_cmcPort),
                     {{"ip", m_cmcIp}, {"port", m_cmcPort}});
        emit cmcConnectionChanged(true, QString());
        emit aircraftConnectionChanged(true, m_aircraftCode, QString());
    }
}

void DeviceConnector::onDisconnected()
{
    Logger::warn("AIRCRAFT_DISCONNECTED",
                 QString("飞机与地面站断开连接 %1:%2").arg(m_cmcIp).arg(m_cmcPort),
                 {{"ip", m_cmcIp}, {"port", m_cmcPort}});

    m_isConnected = false;
    const QString disconnectedAircraftCode = m_aircraftCode;
    m_aircraftCode.clear();
    m_deviceStatusMap.clear();
    cleanupFileTransfer();

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
        if (socket == m_socket) {
            m_socket = nullptr;
        }
    }

    emit cmcConnectionChanged(false, "飞机连接已断开");
    emit aircraftConnectionChanged(false, disconnectedAircraftCode, "飞机连接已断开");
}

void DeviceConnector::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    const QString errorMsg = socket ? socket->errorString() : QString("飞机 socket 不存在");
    Logger::error("AIRCRAFT_NETWORK_ERROR",
                  "飞机连接发生网络错误",
                  {{"ip", m_cmcIp}, {"port", m_cmcPort}, {"error_code", static_cast<int>(socketError)}, {"error_message", errorMsg}});

    m_isConnected = false;
    emit cmcConnectionChanged(false, errorMsg);
    emit aircraftConnectionChanged(false, m_aircraftCode, errorMsg);
}

void DeviceConnector::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || socket != m_socket) {
        return;
    }

    m_receiveBuffer.append(socket->readAll());

    Command cmd;
    QByteArray payload;

    while (parsePacket(m_receiveBuffer, cmd, payload)) {
        switch (cmd) {
        case Command::AircraftHello:
            handleAircraftHello(payload);
            break;
        case Command::DeviceStatusFull:
            handleDeviceStatusFull(payload);
            break;
        case Command::DeviceStatusUpdate:
            handleDeviceStatusUpdate(payload);
            break;
        case Command::FileReceiveResult:
            handleFileReceiveResult(payload);
            break;
        case Command::InstallResult:
            handleInstallResult(payload);
            break;
        case Command::Error:
            Logger::error("AIRCRAFT_MESSAGE_ERROR", QString::fromUtf8(payload));
            break;
        default:
            Logger::warn("AIRCRAFT_MESSAGE_IGNORED",
                         "收到未知飞机命令",
                         {{"command", static_cast<int>(cmd)}});
            break;
        }
    }
}

void DeviceConnector::onSendFileData()
{
    if (!m_currentFile || !m_currentFile->isOpen()) {
        Logger::warn("TRANSFER_SEND_FAILED", "没有正在进行的文件传输", __FILE__, Q_FUNC_INFO);
        return;
    }

    QByteArray data = m_currentFile->read(SEND_BUFFER_SIZE);

    if (data.isEmpty()) {
        if (m_sentBytes == m_fileSize) {
            if (sendFileEnd()) {
                Logger::info("TRANSFER_FILE_SENT",
                             "文件数据已发送完成，等待飞机接收结果",
                             __FILE__, Q_FUNC_INFO,
                             {{"device_task_id", m_currentTaskId}, {"sent_bytes", m_sentBytes}, {"total_size", m_fileSize}});
            } else {
                Logger::error("TRANSFER_SEND_FAILED",
                              "发送文件结束命令失败",
                              __FILE__, Q_FUNC_INFO,
                              {{"device_task_id", m_currentTaskId}});
                emit sendFinished(m_currentTaskId, false, "发送文件结束命令失败");
                cleanupFileTransfer();
            }
        } else {
            Logger::error("TRANSFER_SEND_FAILED",
                          "文件读取错误",
                          __FILE__, Q_FUNC_INFO,
                          {{"device_task_id", m_currentTaskId}, {"sent_bytes", m_sentBytes}, {"expected_size", m_fileSize}});
            emit sendFinished(m_currentTaskId, false, "文件读取错误");
            cleanupFileTransfer();
        }
        return;
    }

    if (sendCommand(Command::FileData, data)) {
        m_sentBytes += data.size();
        const int progress = m_fileSize > 0 ? static_cast<int>((m_sentBytes * 100) / m_fileSize) : 100;

        emit sendProgress(m_currentTaskId, m_sentBytes, m_fileSize, progress);
        if (m_sendTimer) {
            m_sendTimer->start(SEND_INTERVAL_MS);
        }
    } else {
        Logger::error("TRANSFER_SEND_FAILED",
                      "发送文件数据失败",
                      __FILE__, Q_FUNC_INFO,
                      {{"device_task_id", m_currentTaskId}, {"sent_bytes", m_sentBytes}, {"total_size", m_fileSize}});
        emit sendFinished(m_currentTaskId, false, "发送数据失败");
        cleanupFileTransfer();
    }
}

bool DeviceConnector::sendCommand(Command cmd, const QByteArray& data)
{
    QTcpSocket* socket = clientSocket();
    if (!socket) {
        Logger::warn("AIRCRAFT_COMMAND_SEND_FAILED",
                     "飞机 socket 未连接，无法发送命令",
                     {{"command", static_cast<int>(cmd)}});
        return false;
    }

    const QByteArray packet = buildPacket(cmd, data);
    const qint64 written = socket->write(packet);

    if (written != packet.size()) {
        Logger::warn("AIRCRAFT_COMMAND_SEND_FAILED",
                     "飞机命令写入不完整",
                     {{"command", static_cast<int>(cmd)}, {"written", written}, {"expected", packet.size()}});
        return false;
    }

    return socket->flush();
}

bool DeviceConnector::sendFileStart(const QString& taskId,
                                    const QString& targetDeviceId,
                                    const QString& fileName,
                                    qint64 fileSize,
                                    const QString& sha256)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    const QByteArray taskIdBytes = taskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    const QByteArray deviceIdBytes = targetDeviceId.toUtf8();
    stream << static_cast<quint16>(deviceIdBytes.size());
    stream.writeRawData(deviceIdBytes.data(), deviceIdBytes.size());

    const QByteArray fileNameBytes = fileName.toUtf8();
    stream << static_cast<quint16>(fileNameBytes.size());
    stream.writeRawData(fileNameBytes.data(), fileNameBytes.size());

    stream << static_cast<quint64>(fileSize);

    const QByteArray sha256Bytes = sha256.toUtf8();
    stream << static_cast<quint16>(sha256Bytes.size());
    stream.writeRawData(sha256Bytes.data(), sha256Bytes.size());

    Logger::info("TRANSFER_FILE_START",
                 "发送文件开始命令",
                 __FILE__, Q_FUNC_INFO,
                 {{"device_task_id", taskId}, {"target_device_id", targetDeviceId}, {"file_name", fileName}, {"file_size", fileSize}, {"sha256", sha256}});

    return sendCommand(Command::FileStart, data);
}

bool DeviceConnector::sendFileEnd()
{
    Logger::info("TRANSFER_FILE_END", "发送文件结束命令", __FILE__, Q_FUNC_INFO, {{"device_task_id", m_currentTaskId}});
    return sendCommand(Command::FileEnd);
}

bool DeviceConnector::sendInstallStart(const QString& aircraftTaskId)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    const QByteArray taskIdBytes = aircraftTaskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    Logger::info("INSTALL_START",
                 "发送统一安装开始命令",
                 __FILE__, Q_FUNC_INFO,
                 {{"aircraft_task_id", aircraftTaskId}});
    return sendCommand(Command::InstallStart, data);
}

QByteArray DeviceConnector::buildPacket(Command cmd, const QByteArray& payload)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << PACKET_START_MARK;
    stream << static_cast<quint8>(cmd);
    stream << static_cast<quint32>(payload.size());

    if (!payload.isEmpty()) {
        stream.writeRawData(payload.data(), payload.size());
    }

    quint16 checksum = 0;
    for (char c : packet) {
        checksum ^= static_cast<quint8>(c);
    }
    stream << checksum;

    return packet;
}

bool DeviceConnector::parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload)
{
    if (data.size() < PACKET_HEADER_SIZE) {
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 startMark;
    stream >> startMark;

    if (startMark != PACKET_START_MARK) {
        Logger::warn("AIRCRAFT_PACKET_INVALID", "飞机数据包起始标志无效", {{"start_mark", startMark}});
        m_receiveBuffer.remove(0, 1);
        return false;
    }

    quint8 cmdByte;
    stream >> cmdByte;
    cmd = static_cast<Command>(cmdByte);

    quint32 payloadSize;
    stream >> payloadSize;

    const int totalSize = PACKET_HEADER_SIZE + payloadSize;
    if (data.size() < totalSize) {
        return false;
    }

    payload.resize(payloadSize);
    if (payloadSize > 0) {
        stream.readRawData(payload.data(), payloadSize);
    }

    quint16 receivedChecksum;
    stream >> receivedChecksum;

    quint16 calculatedChecksum = 0;
    for (int i = 0; i < totalSize - 2; ++i) {
        calculatedChecksum ^= static_cast<quint8>(data[i]);
    }

    if (calculatedChecksum != receivedChecksum) {
        Logger::warn("AIRCRAFT_PACKET_INVALID",
                     "飞机数据包校验和不匹配",
                     {{"expected_checksum", calculatedChecksum}, {"received_checksum", receivedChecksum}});
        m_receiveBuffer.clear();
        return false;
    }

    m_receiveBuffer.remove(0, totalSize);
    return true;
}

void DeviceConnector::handleAircraftHello(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 aircraftCodeSize = 0;
    stream >> aircraftCodeSize;
    QByteArray aircraftCodeBytes(aircraftCodeSize, Qt::Uninitialized);
    if (aircraftCodeSize > 0) {
        stream.readRawData(aircraftCodeBytes.data(), aircraftCodeSize);
    }

    const QString aircraftCode = QString::fromUtf8(aircraftCodeBytes).trimmed();
    if (aircraftCode.isEmpty()) {
        Logger::warn("AIRCRAFT_HELLO_INVALID",
                     "飞机连接握手未包含飞机编号",
                     {{"ip", m_cmcIp}, {"port", m_cmcPort}});
        return;
    }

    m_aircraftCode = aircraftCode;
    Logger::info("AIRCRAFT_IDENTIFIED",
                 QString("已识别连接飞机：%1").arg(m_aircraftCode),
                 {{"aircraft_code", m_aircraftCode}, {"ip", m_cmcIp}, {"port", m_cmcPort}});
    emit aircraftConnectionChanged(true, m_aircraftCode, QString());
}

void DeviceConnector::handleDeviceStatusFull(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 deviceCount;
    stream >> deviceCount;

    Logger::info("AIRCRAFT_DEVICE_STATUS_FULL",
                 "收到飞机全量设备状态",
                 {{"device_count", deviceCount}});

    QList<DeviceStatus> devices;
    m_deviceStatusMap.clear();

    for (int i = 0; i < deviceCount; ++i) {
        DeviceStatus status;

        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        quint16 deviceNameSize;
        stream >> deviceNameSize;
        QByteArray deviceNameBytes(deviceNameSize, Qt::Uninitialized);
        stream.readRawData(deviceNameBytes.data(), deviceNameSize);
        status.deviceName = QString::fromUtf8(deviceNameBytes);

        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        quint16 versionSize;
        stream >> versionSize;
        if (versionSize > 0) {
            QByteArray versionBytes(versionSize, Qt::Uninitialized);
            stream.readRawData(versionBytes.data(), versionSize);
            status.version = QString::fromUtf8(versionBytes);
        }

        quint64 timestamp;
        stream >> timestamp;
        status.lastUpdateTime = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        m_deviceStatusMap[status.deviceId] = status;
        devices.append(status);
    }

    emit deviceStatusFullUpdated(devices);
}

void DeviceConnector::handleDeviceStatusUpdate(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 updateCount;
    stream >> updateCount;

    Logger::debug("AIRCRAFT_DEVICE_STATUS_UPDATE",
                  "收到飞机增量设备状态",
                  {{"update_count", updateCount}});

    QList<DeviceStatus> updates;

    for (int i = 0; i < updateCount; ++i) {
        DeviceStatus status;

        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        quint64 timestamp;
        stream >> timestamp;
        status.lastUpdateTime = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        if (m_deviceStatusMap.contains(status.deviceId)) {
            m_deviceStatusMap[status.deviceId].isOnline = status.isOnline;
            m_deviceStatusMap[status.deviceId].lastUpdateTime = status.lastUpdateTime;
            status.deviceName = m_deviceStatusMap[status.deviceId].deviceName;
            status.version = m_deviceStatusMap[status.deviceId].version;
        } else {
            Logger::warn("AIRCRAFT_DEVICE_STATUS_UNKNOWN",
                         "收到未知设备的状态更新",
                         {{"device_code", status.deviceId}});
            continue;
        }

        updates.append(status);
    }

    emit deviceStatusIncrementalUpdated(updates);
}

void DeviceConnector::handleFileReceiveResult(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    const QString taskId = QString::fromUtf8(taskIdBytes);

    quint8 success;
    stream >> success;

    quint16 msgSize;
    stream >> msgSize;
    QByteArray msgBytes(msgSize, Qt::Uninitialized);
    stream.readRawData(msgBytes.data(), msgSize);
    const QString message = QString::fromUtf8(msgBytes);

    Logger::info(success == 1 ? "TRANSFER_ACCEPTED" : "TRANSFER_REJECTED",
                 success == 1 ? "飞机已接收文件" : "飞机拒绝接收文件",
                 __FILE__, Q_FUNC_INFO,
                 {{"device_task_id", taskId}, {"message", message}});

    emit sendFinished(taskId, success == 1, message);

    if (success != 1) {
        cleanupFileTransfer();
    }
}

void DeviceConnector::handleInstallResult(const QByteArray& payload)
{
    cleanupFileTransfer();

    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    const QString taskId = QString::fromUtf8(taskIdBytes);

    quint16 deviceIdSize;
    stream >> deviceIdSize;
    QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
    stream.readRawData(deviceIdBytes.data(), deviceIdSize);
    const QString deviceId = QString::fromUtf8(deviceIdBytes);

    quint8 success;
    stream >> success;

    quint16 msgSize;
    stream >> msgSize;
    QByteArray msgBytes(msgSize, Qt::Uninitialized);
    stream.readRawData(msgBytes.data(), msgSize);
    const QString message = QString::fromUtf8(msgBytes);

    Logger::info(success == 1 ? "INSTALL_RESULT_SUCCESS" : "INSTALL_RESULT_FAILED",
                 success == 1 ? "收到设备安装成功结果" : "收到设备安装失败结果",
                 __FILE__, Q_FUNC_INFO,
                 {{"device_task_id", taskId}, {"device_code", deviceId}, {"message", message}});

    emit installResult(taskId, deviceId, success == 1, message);
}

void DeviceConnector::cleanupFileTransfer()
{
    if (m_sendTimer) {
        m_sendTimer->stop();
    }

    if (m_currentFile) {
        if (m_currentFile->isOpen()) {
            m_currentFile->close();
        }
        m_currentFile->deleteLater();
        m_currentFile = nullptr;
    }

    m_currentTaskId.clear();
    m_currentTargetDeviceId.clear();
    m_fileSize = 0;
    m_sentBytes = 0;
    m_fileSha256.clear();
}

void DeviceConnector::cleanupClientConnection()
{
    if (!m_socket) {
        return;
    }

    disconnect(m_socket, nullptr, this, nullptr);
    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ClosingState) {
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected(1000);
    }
    m_socket->deleteLater();
    m_socket = nullptr;
    m_isConnected = false;
    m_receiveBuffer.clear();
    m_deviceStatusMap.clear();
    m_aircraftCode.clear();
}

QTcpSocket* DeviceConnector::clientSocket() const
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        return m_socket;
    }
    return nullptr;
}

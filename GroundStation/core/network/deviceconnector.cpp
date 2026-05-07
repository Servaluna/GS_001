#include "deviceconnector.h"

#include <QTimer>
#include <QDateTime>

DeviceConnector::DeviceConnector(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_cmcPort(0)
    , m_isConnected(false)
    , m_currentFile(nullptr)
    , m_fileSize(0)
    , m_sentBytes(0)
{
    // setAttribute(Qt::WA_DeleteOnClose);

    // this->setWindowTitle("连接设备");

    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected,
            this, &DeviceConnector::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &DeviceConnector::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &DeviceConnector::onErrorOccurred);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &DeviceConnector::onReadyRead);
}

DeviceConnector::~DeviceConnector()
{
    cleanupFileTransfer();
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }
}

bool DeviceConnector::connectToCMC(const QString& ip, quint16 port)
{
    if (m_isConnected) {
        qDebug() << "DeviceConnector: Already connected to CMC";
        return true;
    }

    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        qDebug() << "DeviceConnector: Already connecting to CMC";
        return false;
    }

    m_cmcIp = ip;
    m_cmcPort = port;

    qDebug() << "DeviceConnector: Connecting to CMC at" << ip << ":" << port;
    m_socket->connectToHost(ip, port);
    return true;
}

void DeviceConnector::disconnectFromCMC()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

bool DeviceConnector::isConnected() const
{
    return m_isConnected;
}

// ==================== 文件传输 ====================

void DeviceConnector::sendFileToDevice(const QString& taskId,
                                       const QString& targetDeviceId,
                                       const QString& localPath,
                                       const QString& fileName,
                                       const QString& sha256)
{
    if (!m_isConnected) {
        qWarning() << "DeviceConnector: Not connected to CMC, cannot send file";
        emit sendFinished(taskId, false, "未连接到 CMC");
        return;
    }

    // 检查目标设备是否在线，从本地缓存查询。
    if (!isDeviceOnline(targetDeviceId)) {
        qWarning() << "DeviceConnector: Target device not online:" << targetDeviceId;
        emit sendFinished(taskId, false, QString("目标设备 %1 不在线").arg(targetDeviceId));
        return;
    }

    // 检查本地文件
    QFile* file = new QFile(localPath, this);
    if (!file->exists()) {
        qWarning() << "DeviceConnector: File not found:" << localPath;
        emit sendFinished(taskId, false, "本地文件不存在");
        delete file;
        return;
    }

    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "DeviceConnector: Cannot open file:" << localPath;
        emit sendFinished(taskId, false, "无法打开本地文件");
        delete file;
        return;
    }

    // 清理之前的传输，正常情况下应该已经清理过。
    cleanupFileTransfer();

    // 设置当前传输上下文
    m_currentTaskId = taskId;
    m_currentTargetDeviceId = targetDeviceId;
    m_currentFile = file;
    m_fileSize = file->size();
    m_sentBytes = 0;
    m_fileSha256 = sha256;

    qDebug() << "DeviceConnector: Starting file send - taskId:" << taskId
             << "targetDevice:" << targetDeviceId
             << "fileName:" << fileName
             << "fileSize:" << m_fileSize
             << "sha256:" << sha256;

    // 发送文件开始命令
    if (!sendFileStart(taskId, targetDeviceId, fileName, m_fileSize, sha256)) {
        qCritical() << "DeviceConnector: Failed to send FileStart command";
        emit sendFinished(taskId, false, "发送文件开始命令失败");
        cleanupFileTransfer();
        return;
    }

    // 开始发送文件数据，异步分片发送。
    QTimer::singleShot(SEND_INTERVAL_MS, this, &DeviceConnector::onSendFileData);
}

// ==================== 设备状态查询 ====================

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
    if (it != m_deviceStatusMap.end()) {
        return it.value().isOnline;
    }
    return false;
}

// ==================== 私有槽函数 ====================

void DeviceConnector::onConnected()
{
    m_isConnected = true;
    qDebug() << "DeviceConnector: Connected to CMC successfully";
    emit cmcConnectionChanged(true, QString());
}

void DeviceConnector::onDisconnected()
{
    m_isConnected = false;
    qDebug() << "DeviceConnector: Disconnected from CMC";

    // 清理所有状态
    m_deviceStatusMap.clear();
    cleanupFileTransfer();

    emit cmcConnectionChanged(false, "与 CMC 的连接已断开");
}

void DeviceConnector::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    QString errorMsg = m_socket->errorString();
    qWarning() << "DeviceConnector: Socket error:" << errorMsg << "(code:" << socketError << ")";

    m_isConnected = false;
    emit cmcConnectionChanged(false, errorMsg);
}

void DeviceConnector::onReadyRead()
{
    // 追加数据到接收缓冲区
    m_receiveBuffer.append(m_socket->readAll());

    // 循环处理缓冲区中的所有完整数据包
    Command cmd;
    QByteArray payload;

    while (parsePacket(m_receiveBuffer, cmd, payload)) {
        qDebug() << "DeviceConnector: Received command:" << static_cast<int>(cmd);

        switch (cmd) {
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
            qWarning() << "DeviceConnector: Received error from CMC:" << QString::fromUtf8(payload);
            break;

        default:
            qWarning() << "DeviceConnector: Unknown command:" << static_cast<int>(cmd);
            break;
        }
    }
}

void DeviceConnector::onSendFileData()
{
    if (!m_currentFile || !m_currentFile->isOpen()) {
        qWarning() << "DeviceConnector: No active file transfer";
        return;
    }

    // 读取一块数据
    QByteArray data = m_currentFile->read(SEND_BUFFER_SIZE);

    if (data.isEmpty()) {
        // 文件读取完成，发送 FileEnd 命令。
        if (m_sentBytes == m_fileSize) {
            qDebug() << "DeviceConnector: File read complete, sent:" << m_sentBytes
                     << "of" << m_fileSize;

            if (sendFileEnd()) {
                qDebug() << "DeviceConnector: FileEnd command sent, waiting for CMC response";
            } else {
                qCritical() << "DeviceConnector: Failed to send FileEnd command";
                emit sendFinished(m_currentTaskId, false, "发送文件结束命令失败");
                cleanupFileTransfer();
            }
        } else {
            // 文件读取错误
            qCritical() << "DeviceConnector: File read error, sent:" << m_sentBytes
                        << "expected:" << m_fileSize;
            emit sendFinished(m_currentTaskId, false, "文件读取错误");
            cleanupFileTransfer();
        }
        return;
    }

    // 发送数据块
    if (sendCommand(Command::FileData, data)) {
        m_sentBytes += data.size();
        int progress = static_cast<int>((m_sentBytes * 100) / m_fileSize);

        qDebug() << "DeviceConnector: Sent data chunk, progress:" << progress
                 << "% (" << m_sentBytes << "/" << m_fileSize << ")";

        emit sendProgress(m_currentTaskId, m_sentBytes, m_fileSize, progress);

        // 继续发送下一块
        QTimer::singleShot(SEND_INTERVAL_MS, this, &DeviceConnector::onSendFileData);
    } else {
        qCritical() << "DeviceConnector: Failed to send data chunk";
        emit sendFinished(m_currentTaskId, false, "发送数据失败");
        cleanupFileTransfer();
    }
}

// ==================== 数据包发送 ====================

bool DeviceConnector::sendCommand(Command cmd, const QByteArray& data)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "DeviceConnector: Socket not connected, cannot send command";
        return false;
    }

    QByteArray packet = buildPacket(cmd, data);
    qint64 written = m_socket->write(packet);

    if (written != packet.size()) {
        qWarning() << "DeviceConnector: Incomplete write, wrote:" << written
                   << "expected:" << packet.size();
        return false;
    }

    return true;
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

    // 任务 ID
    QByteArray taskIdBytes = taskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    // 目标设备 ID
    QByteArray deviceIdBytes = targetDeviceId.toUtf8();
    stream << static_cast<quint16>(deviceIdBytes.size());
    stream.writeRawData(deviceIdBytes.data(), deviceIdBytes.size());

    // 文件名
    QByteArray fileNameBytes = fileName.toUtf8();
    stream << static_cast<quint16>(fileNameBytes.size());
    stream.writeRawData(fileNameBytes.data(), fileNameBytes.size());

    // 文件大小
    stream << static_cast<quint64>(fileSize);

    // SHA-256
    QByteArray sha256Bytes = sha256.toUtf8();
    stream << static_cast<quint16>(sha256Bytes.size());
    stream.writeRawData(sha256Bytes.data(), sha256Bytes.size());

    qDebug() << "DeviceConnector: Sending FileStart - taskId:" << taskId
             << "targetDevice:" << targetDeviceId
             << "fileName:" << fileName
             << "size:" << fileSize
             << "sha256:" << sha256;

    return sendCommand(Command::FileStart, data);
}

bool DeviceConnector::sendFileEnd()
{
    qDebug() << "DeviceConnector: Sending FileEnd for task:" << m_currentTaskId;
    return sendCommand(Command::FileEnd);
}

// ==================== 数据包构造与解析 ====================

QByteArray DeviceConnector::buildPacket(Command cmd, const QByteArray& payload)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 起始标志
    stream << PACKET_START_MARK;

    // 命令
    stream << static_cast<quint8>(cmd);

    // 数据长度
    stream << static_cast<quint32>(payload.size());

    // 数据
    if (!payload.isEmpty()) {
        stream.writeRawData(payload.data(), payload.size());
    }

    // 校验和，异或所有字节。
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
        return false;  // 数据不足，等待更多数据。
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    // 读取起始标志
    quint16 startMark;
    stream >> startMark;

    if (startMark != PACKET_START_MARK) {
        qWarning() << "DeviceConnector: Invalid packet start mark:" << startMark;
        // 跳过这个字节，尝试重新同步。
        m_receiveBuffer.remove(0, 1);
        return false;
    }

    // 读取命令
    quint8 cmdByte;
    stream >> cmdByte;
    cmd = static_cast<Command>(cmdByte);

    // 读取数据长度
    quint32 payloadSize;
    stream >> payloadSize;

    // 计算完整包大小
    int totalSize = PACKET_HEADER_SIZE + payloadSize + 2;  // +2 为校验和

    if (data.size() < totalSize) {
        return false;  // 数据不完整，等待更多数据。
    }

    // 读取数据
    payload.resize(payloadSize);
    if (payloadSize > 0) {
        stream.readRawData(payload.data(), payloadSize);
    }

    // 读取校验和
    quint16 receivedChecksum;
    stream >> receivedChecksum;

    // 验证校验和，不包含最后 2 字节。
    quint16 calculatedChecksum = 0;
    for (int i = 0; i < totalSize - 2; ++i) {
        calculatedChecksum ^= static_cast<quint8>(data[i]);
    }

    if (calculatedChecksum != receivedChecksum) {
        qWarning() << "DeviceConnector: Checksum mismatch, expected:"
                   << calculatedChecksum << "got:" << receivedChecksum;
        m_receiveBuffer.clear();
        return false;
    }

    // 移除已处理的数据
    m_receiveBuffer.remove(0, totalSize);

    return true;
}

// ==================== 命令处理 ====================

void DeviceConnector::handleDeviceStatusFull(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 deviceCount;
    stream >> deviceCount;

    qDebug() << "DeviceConnector: Received full device status, count:" << deviceCount;

    QList<DeviceStatus> devices;
    m_deviceStatusMap.clear();

    for (int i = 0; i < deviceCount; ++i) {
        DeviceStatus status;

        // 设备 ID
        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        // 设备名称
        quint16 deviceNameSize;
        stream >> deviceNameSize;
        QByteArray deviceNameBytes(deviceNameSize, Qt::Uninitialized);
        stream.readRawData(deviceNameBytes.data(), deviceNameSize);
        status.deviceName = QString::fromUtf8(deviceNameBytes);

        // 在线状态
        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        // 版本号
        quint16 versionSize;
        stream >> versionSize;
        if (versionSize > 0) {
            QByteArray versionBytes(versionSize, Qt::Uninitialized);
            stream.readRawData(versionBytes.data(), versionSize);
            status.version = QString::fromUtf8(versionBytes);
        }

        // 最后更新时间，使用 CMC 提供的时间戳。
        quint64 timestamp;
        stream >> timestamp;
        status.lastUpdateTime = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        m_deviceStatusMap[status.deviceId] = status;
        devices.append(status);

        qDebug() << "  Device:" << status.deviceId
                 << "name:" << status.deviceName
                 << "online:" << status.isOnline
                 << "version:" << status.version;
    }

    emit deviceStatusFullUpdated(devices);
}

void DeviceConnector::handleDeviceStatusUpdate(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 updateCount;
    stream >> updateCount;

    qDebug() << "DeviceConnector: Received device status update, count:" << updateCount;

    QList<DeviceStatus> updates;

    for (int i = 0; i < updateCount; ++i) {
        DeviceStatus status;

        // 设备 ID
        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        // 在线状态
        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        // 最后更新时间
        quint64 timestamp;
        stream >> timestamp;
        status.lastUpdateTime = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        // 更新本地缓存
        if (m_deviceStatusMap.contains(status.deviceId)) {
            m_deviceStatusMap[status.deviceId].isOnline = status.isOnline;
            m_deviceStatusMap[status.deviceId].lastUpdateTime = status.lastUpdateTime;

            // 保留原有信息
            status.deviceName = m_deviceStatusMap[status.deviceId].deviceName;
            status.version = m_deviceStatusMap[status.deviceId].version;
        } else {
            qWarning() << "DeviceConnector: Unknown device in update:" << status.deviceId;
            // 没有基础信息，跳过。
            continue;
        }

        updates.append(status);

        qDebug() << "  Device:" << status.deviceId
                 << "online:" << status.isOnline
                 << "time:" << status.lastUpdateTime;
    }

    emit deviceStatusIncrementalUpdated(updates);
}

void DeviceConnector::handleFileReceiveResult(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    // 任务 ID
    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    QString taskId = QString::fromUtf8(taskIdBytes);

    // 成功标志
    quint8 success;
    stream >> success;

    // 消息
    quint16 msgSize;
    stream >> msgSize;
    QByteArray msgBytes(msgSize, Qt::Uninitialized);
    stream.readRawData(msgBytes.data(), msgSize);
    QString message = QString::fromUtf8(msgBytes);

    qDebug() << "DeviceConnector: File receive result - taskId:" << taskId
             << "success:" << (success == 1)
             << "message:" << message;

    emit sendFinished(taskId, success == 1, message);

    if (success == 1) {
        qDebug() << "DeviceConnector: File accepted by CMC, waiting for install result from device";
    } else {
        // 文件接收失败，清理传输资源。
        cleanupFileTransfer();
    }
}

void DeviceConnector::handleInstallResult(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    // 任务 ID
    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    QString taskId = QString::fromUtf8(taskIdBytes);

    // 设备 ID
    quint16 deviceIdSize;
    stream >> deviceIdSize;
    QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
    stream.readRawData(deviceIdBytes.data(), deviceIdSize);
    QString deviceId = QString::fromUtf8(deviceIdBytes);

    // 成功标志
    quint8 success;
    stream >> success;

    // 消息
    quint16 msgSize;
    stream >> msgSize;
    QByteArray msgBytes(msgSize, Qt::Uninitialized);
    stream.readRawData(msgBytes.data(), msgSize);
    QString message = QString::fromUtf8(msgBytes);

    qDebug() << "DeviceConnector: Install result - taskId:" << taskId
             << "deviceId:" << deviceId
             << "success:" << (success == 1)
             << "message:" << message;

    emit installResult(taskId, deviceId, success == 1, message);

    // 安装完成，无论成功或失败都清理传输资源。
    cleanupFileTransfer();
}

void DeviceConnector::cleanupFileTransfer()
{
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

    qDebug() << "DeviceConnector: File transfer cleaned up";
}






// void DeviceConnector::on_btnCancel_clicked()
// {
//     this -> close();
// }

// void DeviceConnector::on_btnConnect_clicked()
// {
//     QString IP = ui ->lineEditIP ->text();
//     QString Port = ui ->lineEditPort ->text();

//     m_socket ->connectToHost(QHostAddress(IP),Port.toUShort());

//     connect(m_socket , &QTcpSocket::connected,[this](){
//         QMessageBox::information(this, "连接提示", "连接服务器成功");
//     });
//     connect(m_socket , &QTcpSocket::disconnected,[this](){
//         QMessageBox::warning(this, "连接提示", "网络异常，连接失败");
//     });
// }

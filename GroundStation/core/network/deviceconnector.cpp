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

    // this->setWindowTitle("杩炴帴璁惧");

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

// ==================== 鏂囦欢浼犺緭 ====================

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

    // 妫€鏌ョ洰鏍囪澶囨槸鍚﹀湪绾匡紙浠庢湰鍦扮紦瀛樻煡璇級
    if (!isDeviceOnline(targetDeviceId)) {
        qWarning() << "DeviceConnector: Target device not online:" << targetDeviceId;
        emit sendFinished(taskId, false, QString("目标设备 %1 不在线").arg(targetDeviceId));
        return;
    }

    // 妫€鏌ユ湰鍦版枃浠?
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

    // 娓呯悊涔嬪墠鐨勪紶杈擄紙姝ｅ父鎯呭喌涓嬪簲璇ュ凡缁忔竻鐞嗕簡锛?
    cleanupFileTransfer();

    // 璁剧疆褰撳墠浼犺緭涓婁笅鏂?
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

    // 寮€濮嬪彂閫佹枃浠舵暟鎹紙寮傛鍒嗙墖鍙戦€侊級
    QTimer::singleShot(SEND_INTERVAL_MS, this, &DeviceConnector::onSendFileData);
}

// ==================== 璁惧鐘舵€佹煡璇?====================

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

// ==================== 绉佹湁妲藉嚱鏁?====================

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

    // 娓呯悊鎵€鏈夌姸鎬?
    m_deviceStatusMap.clear();
    cleanupFileTransfer();

    emit cmcConnectionChanged(false, "涓嶤MC鐨勮繛鎺ュ凡鏂紑");
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
    // 杩藉姞鏁版嵁鍒版帴鏀剁紦鍐插尯
    m_receiveBuffer.append(m_socket->readAll());

    // 寰幆澶勭悊缂撳啿鍖轰腑鐨勬墍鏈夊畬鏁存暟鎹寘
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

    // 璇诲彇涓€鍧楁暟鎹?
    QByteArray data = m_currentFile->read(SEND_BUFFER_SIZE);

    if (data.isEmpty()) {
        // 鏂囦欢璇诲彇瀹屾垚锛屽彂閫?FileEnd 鍛戒护
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
            // 鏂囦欢璇诲彇閿欒
            qCritical() << "DeviceConnector: File read error, sent:" << m_sentBytes
                        << "expected:" << m_fileSize;
            emit sendFinished(m_currentTaskId, false, "文件读取错误");
            cleanupFileTransfer();
        }
        return;
    }

    // 鍙戦€佹暟鎹潡
    if (sendCommand(Command::FileData, data)) {
        m_sentBytes += data.size();
        int progress = static_cast<int>((m_sentBytes * 100) / m_fileSize);

        qDebug() << "DeviceConnector: Sent data chunk, progress:" << progress
                 << "% (" << m_sentBytes << "/" << m_fileSize << ")";

        emit sendProgress(m_currentTaskId, m_sentBytes, m_fileSize, progress);

        // 缁х画鍙戦€佷笅涓€鍧?
        QTimer::singleShot(SEND_INTERVAL_MS, this, &DeviceConnector::onSendFileData);
    } else {
        qCritical() << "DeviceConnector: Failed to send data chunk";
        emit sendFinished(m_currentTaskId, false, "发送数据失败");
        cleanupFileTransfer();
    }
}

// ==================== 鏁版嵁鍖呭彂閫?====================

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

    // 浠诲姟ID
    QByteArray taskIdBytes = taskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    // 鐩爣璁惧ID
    QByteArray deviceIdBytes = targetDeviceId.toUtf8();
    stream << static_cast<quint16>(deviceIdBytes.size());
    stream.writeRawData(deviceIdBytes.data(), deviceIdBytes.size());

    // 鏂囦欢鍚?
    QByteArray fileNameBytes = fileName.toUtf8();
    stream << static_cast<quint16>(fileNameBytes.size());
    stream.writeRawData(fileNameBytes.data(), fileNameBytes.size());

    // 鏂囦欢澶у皬
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

// ==================== 鏁版嵁鍖呮瀯閫犱笌瑙ｆ瀽 ====================

QByteArray DeviceConnector::buildPacket(Command cmd, const QByteArray& payload)
{
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 璧峰鏍囧織
    stream << PACKET_START_MARK;

    // 鍛戒护
    stream << static_cast<quint8>(cmd);

    // 鏁版嵁闀垮害
    stream << static_cast<quint32>(payload.size());

    // 鏁版嵁
    if (!payload.isEmpty()) {
        stream.writeRawData(payload.data(), payload.size());
    }

    // 鏍￠獙鍜岋紙寮傛垨鎵€鏈夊瓧鑺傦級
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
        return false;  // 鏁版嵁涓嶈冻锛岀瓑寰呮洿澶氭暟鎹?
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    // 璇诲彇璧峰鏍囧織
    quint16 startMark;
    stream >> startMark;

    if (startMark != PACKET_START_MARK) {
        qWarning() << "DeviceConnector: Invalid packet start mark:" << startMark;
        // 璺宠繃杩欎釜瀛楄妭锛屽皾璇曢噸鏂板悓姝?
        m_receiveBuffer.remove(0, 1);
        return false;
    }

    // 璇诲彇鍛戒护
    quint8 cmdByte;
    stream >> cmdByte;
    cmd = static_cast<Command>(cmdByte);

    // 璇诲彇鏁版嵁闀垮害
    quint32 payloadSize;
    stream >> payloadSize;

    // 璁＄畻瀹屾暣鍖呭ぇ灏?
    int totalSize = PACKET_HEADER_SIZE + payloadSize + 2;  // +2 涓烘牎楠屽拰

    if (data.size() < totalSize) {
        return false;  // 鏁版嵁涓嶅畬鏁达紝绛夊緟鏇村鏁版嵁
    }

    // 璇诲彇鏁版嵁
    payload.resize(payloadSize);
    if (payloadSize > 0) {
        stream.readRawData(payload.data(), payloadSize);
    }

    // 璇诲彇鏍￠獙鍜?
    quint16 receivedChecksum;
    stream >> receivedChecksum;

    // 楠岃瘉鏍￠獙鍜岋紙涓嶅寘鎷渶鍚?瀛楄妭锛?
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

    // 绉婚櫎宸插鐞嗙殑鏁版嵁
    m_receiveBuffer.remove(0, totalSize);

    return true;
}

// ==================== 鍛戒护澶勭悊 ====================

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

        // 璁惧ID
        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        // 璁惧鍚嶇О
        quint16 deviceNameSize;
        stream >> deviceNameSize;
        QByteArray deviceNameBytes(deviceNameSize, Qt::Uninitialized);
        stream.readRawData(deviceNameBytes.data(), deviceNameSize);
        status.deviceName = QString::fromUtf8(deviceNameBytes);

        // 鍦ㄧ嚎鐘舵€?
        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        // 鐗堟湰鍙?
        quint16 versionSize;
        stream >> versionSize;
        if (versionSize > 0) {
            QByteArray versionBytes(versionSize, Qt::Uninitialized);
            stream.readRawData(versionBytes.data(), versionSize);
            status.version = QString::fromUtf8(versionBytes);
        }

        // 鏈€鍚庢洿鏂版椂闂达紙CMC鎻愪緵鐨勬椂闂存埑锛?
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

        // 璁惧ID
        quint16 deviceIdSize;
        stream >> deviceIdSize;
        QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
        stream.readRawData(deviceIdBytes.data(), deviceIdSize);
        status.deviceId = QString::fromUtf8(deviceIdBytes);

        // 鍦ㄧ嚎鐘舵€?
        quint8 online;
        stream >> online;
        status.isOnline = (online == 1);

        // 鏈€鍚庢洿鏂版椂闂?
        quint64 timestamp;
        stream >> timestamp;
        status.lastUpdateTime = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        // 鏇存柊鏈湴缂撳瓨
        if (m_deviceStatusMap.contains(status.deviceId)) {
            m_deviceStatusMap[status.deviceId].isOnline = status.isOnline;
            m_deviceStatusMap[status.deviceId].lastUpdateTime = status.lastUpdateTime;

            // 淇濈暀鍘熸湁淇℃伅
            status.deviceName = m_deviceStatusMap[status.deviceId].deviceName;
            status.version = m_deviceStatusMap[status.deviceId].version;
        } else {
            qWarning() << "DeviceConnector: Unknown device in update:" << status.deviceId;
            // 娌℃湁鍩虹淇℃伅锛岃烦杩?
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

    // 浠诲姟ID
    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    QString taskId = QString::fromUtf8(taskIdBytes);

    // 鎴愬姛鏍囧織
    quint8 success;
    stream >> success;

    // 娑堟伅
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
        // 鏂囦欢鎺ユ敹澶辫触锛屾竻鐞嗕紶杈撹祫婧?
        cleanupFileTransfer();
    }
}

void DeviceConnector::handleInstallResult(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    // 浠诲姟ID
    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    QString taskId = QString::fromUtf8(taskIdBytes);

    // 璁惧ID
    quint16 deviceIdSize;
    stream >> deviceIdSize;
    QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
    stream.readRawData(deviceIdBytes.data(), deviceIdSize);
    QString deviceId = QString::fromUtf8(deviceIdBytes);

    // 鎴愬姛鏍囧織
    quint8 success;
    stream >> success;

    // 娑堟伅
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

    // 瀹夎瀹屾垚锛堟棤璁烘垚鍔熸垨澶辫触锛夛紝娓呯悊浼犺緭璧勬簮
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
//         QMessageBox::information(this,"杩炴帴鎻愮ず","杩炴帴鏈嶅姟鍣ㄦ垚鍔?);
//     });
//     connect(m_socket , &QTcpSocket::disconnected,[this](){
//         QMessageBox::warning(this,"杩炴帴鎻愮ず","缃戠粶寮傚父杩炴帴澶辫触");
//     });
// }

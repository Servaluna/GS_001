#include "filereceiver.h"
#include "installer.h"


#include <QStandardPaths>
#include <Qdir>

FileReceiver::FileReceiver(QObject *parent)
    : QObject{parent}
    , m_server(nullptr)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &FileReceiver::onNewConnection);
}

FileReceiver::~FileReceiver()
{
    stop();
}

bool FileReceiver::start(quint16 port)
{
    if (m_server->isListening()) {
        qWarning() << "FileReceiver: Already listening";
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "FileReceiver: Failed to listen on port" << port
                    << ":" << m_server->errorString();
        return false;
    }

    qDebug() << "FileReceiver: Listening on port" << port;
    return true;
}

void FileReceiver::stop()
{
    if (m_server) {
        m_server->close();
    }

    // 清理所有会话
    for (auto session : m_sessions.values()) {
        cleanupSession(session);
    }
    m_sessions.clear();

    qDebug() << "FileReceiver: Stopped";
}

bool FileReceiver::isReceiving() const
{
    return !m_sessions.isEmpty();
}

void FileReceiver::sendInstallResult(const QString& taskId, bool success, const QString& message)
{
    ReceiveSession* session = m_taskToSession.value(taskId, nullptr);
    if (!session) {
        qWarning() << "FileReceiver: Cannot send install result - session not found for task:" << taskId;
        return;
    }

    if (!session->socket || session->socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "FileReceiver: Cannot send install result - socket not connected";
        return;
    }
    sendInstallResultCommand(session, success, message);

    // 清理会话映射
    // m_taskToSession.remove(taskId);

}

// ==================== 私有槽函数 ====================

void FileReceiver::onNewConnection()
{
    QTcpSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    qDebug() << "FileReceiver: New connection from" << socket->peerAddress().toString();

    ReceiveSession* session = createSession(socket);
    m_sessions[socket] = session;

    connect(socket, &QTcpSocket::readyRead, this, &FileReceiver::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &FileReceiver::onClientDisconnected);
}

void FileReceiver::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    ReceiveSession* session = getSessionBySocket(socket);
    if (!session || !session->isActive) return;

    // 重置超时定时器
    if (session->timeoutTimer) {
        session->timeoutTimer->start(RECEIVE_TIMEOUT_MS);
    }

    // 读取所有可用数据
    QByteArray data = socket->readAll();

    // 追加到缓冲区（需要处理粘包）
    static QMap<QTcpSocket*, QByteArray> buffers;
    buffers[socket].append(data);

    Command cmd;
    QByteArray payload;

    while (parsePacket(buffers[socket], cmd, payload)) {
        switch (cmd) {
        case Command::FileStart:
            handleFileStart(session, payload);
            break;

        case Command::FileData:
            handleFileData(session, payload);
            break;

        case Command::FileEnd:
            handleFileEnd(session);
            break;

        default:
            qWarning() << "FileReceiver: Unknown command:" << static_cast<int>(cmd);
            sendError(session, "未知命令");
            break;
        }
    }
}

void FileReceiver::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qDebug() << "FileReceiver: Client disconnected:" << socket->peerAddress().toString();

    ReceiveSession* session = getSessionBySocket(socket);
    if (session) {
        if (session->isActive && session->receivedBytes < session->fileSize) {
            qWarning() << "FileReceiver: Incomplete transfer, received:"
                       << session->receivedBytes << "of" << session->fileSize;
            emit receiveFailed(session->taskId, "连接断开，文件接收不完整");
        }
        cleanupSession(session);
        m_sessions.remove(socket);
    }

    socket->deleteLater();
}

void FileReceiver::onReceiveTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    // 查找对应的会话
    ReceiveSession* timeoutSession = nullptr;
    for (auto session : m_sessions.values()) {
        if (session->timeoutTimer == timer) {
            timeoutSession = session;
            break;
        }
    }

    if (timeoutSession) {
        qWarning() << "FileReceiver: Receive timeout for task:" << timeoutSession->taskId;
        emit receiveFailed(timeoutSession->taskId, "接收超时");

        if (timeoutSession->socket) {
            timeoutSession->socket->disconnectFromHost();
        }
        cleanupSession(timeoutSession);
        m_sessions.remove(timeoutSession->socket);
    }
}

// ==================== 命令处理 ====================

void FileReceiver::handleFileStart(ReceiveSession* session, const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    // 读取任务ID
    quint16 taskIdSize;
    stream >> taskIdSize;
    QByteArray taskIdBytes(taskIdSize, Qt::Uninitialized);
    stream.readRawData(taskIdBytes.data(), taskIdSize);
    session->taskId = QString::fromUtf8(taskIdBytes);

    // 读取目标设备ID
    quint16 deviceIdSize;
    stream >> deviceIdSize;
    QByteArray deviceIdBytes(deviceIdSize, Qt::Uninitialized);
    stream.readRawData(deviceIdBytes.data(), deviceIdSize);
    session->targetDeviceId = QString::fromUtf8(deviceIdBytes);

    // 读取文件名
    quint16 fileNameSize;
    stream >> fileNameSize;
    QByteArray fileNameBytes(fileNameSize, Qt::Uninitialized);
    stream.readRawData(fileNameBytes.data(), fileNameSize);
    session->fileName = QString::fromUtf8(fileNameBytes);

    // 读取文件大小
    stream >> session->fileSize;

    quint16 sha256Size;
    stream >> sha256Size;
    QByteArray expectedSha256(sha256Size, Qt::Uninitialized);
    stream.readRawData(expectedSha256.data(), sha256Size);
    session->expectedSha256 = QString::fromUtf8(expectedSha256);

    qDebug() << "FileReceiver: FileStart - taskId:" << session->taskId
             << "targetDevice:" << session->targetDeviceId
             << "fileName:" << session->fileName
             << "fileSize:" << session->fileSize
             << "sha256:" << session->expectedSha256;

    // 检查文件大小限制
    if (session->fileSize > MAX_FILE_SIZE) {
        QString error = QString("文件过大: %1 > %2").arg(session->fileSize).arg(MAX_FILE_SIZE);
        qWarning() << "FileReceiver:" << error;
        sendFileReceiveResult(session, false, error);
        return;
    }

    // 创建临时文件
    QString tempPath = getTempFilePath(session->taskId, session->fileName);
    session->tempFile = new QFile(tempPath, this);

    if (!session->tempFile->open(QIODevice::WriteOnly)) {
        QString error = QString("无法创建临时文件: %1").arg(tempPath);
        qCritical() << "FileReceiver:" << error;
        sendFileReceiveResult(session, false, error);
        return;
    }

    session->hash = new QCryptographicHash(QCryptographicHash::Sha256);
    session->receivedBytes = 0;
    session->isActive = true;

    // 启动超时定时器
    session->timeoutTimer = new QTimer(this);
    connect(session->timeoutTimer, &QTimer::timeout, this, &FileReceiver::onReceiveTimeout);
    session->timeoutTimer->start(RECEIVE_TIMEOUT_MS);

    m_taskToSession[session->taskId] = session;

    qDebug() << "FileReceiver: Ready to receive file, temp file:" << tempPath;
}

void FileReceiver::handleFileData(ReceiveSession* session, const QByteArray& payload)
{
    if (!session->isActive) {
        qWarning() << "FileReceiver: Received data but session not active";
        return;
    }

    // 写入文件
    qint64 bytesWritten = session->tempFile->write(payload);
    if (bytesWritten != payload.size()) {
        qCritical() << "FileReceiver: Failed to write to temp file";
        sendFileReceiveResult(session, false, "写入临时文件失败");
        return;
    }

    // 更新 SHA-256
    session->hash->addData(payload);

    // 更新进度
    session->receivedBytes += payload.size();
    int percent = static_cast<int>((session->receivedBytes * 100) / session->fileSize);

    emit receiveProgress(session->taskId, session->receivedBytes, session->fileSize, percent);

    qDebug() << "FileReceiver: Received data chunk, progress:" << percent
             << "% (" << session->receivedBytes << "/" << session->fileSize << ")";
}

void FileReceiver::handleFileEnd(ReceiveSession* session)
{
    if (!session->isActive) {
        qWarning() << "FileReceiver: Received FileEnd but session not active";
        return;
    }

    qDebug() << "FileReceiver: FileEnd received, received:" << session->receivedBytes
             << "expected:" << session->fileSize;

    // 检查文件大小
    if (session->receivedBytes != session->fileSize) {
        QString error = QString("文件大小不匹配: 收到 %1, 期望 %2")
                            .arg(session->receivedBytes).arg(session->fileSize);
        qWarning() << "FileReceiver:" << error;
        sendFileReceiveResult(session, false, error);
        cleanupSession(session);
        return;
    }

    QByteArray actualSha256 = session->hash->result().toHex();
    session->tempFile->close();

    if (QString::fromUtf8(actualSha256).compare(session->expectedSha256, Qt::CaseInsensitive) != 0) {
        QString error = QString("文件 SHA-256 不匹配: 收到 %1, 期望 %2")
                            .arg(QString::fromUtf8(actualSha256), session->expectedSha256);
        qWarning() << "FileReceiver:" << error;
        sendFileReceiveResult(session, false, error);
        cleanupSession(session);
        return;
    }

    qDebug() << "FileReceiver: File received successfully, SHA-256:" << actualSha256;

    // 通知地面站接收成功
    sendFileReceiveResult(session, true, "文件接收成功");

    // 通知安装器进行安装
    notifyInstaller(session);
}

// ==================== 响应发送 ====================

void FileReceiver::sendFileReceiveResult(ReceiveSession* session, bool success, const QString& message)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 任务ID
    QByteArray taskIdBytes = session->taskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    // 成功标志
    stream << static_cast<quint8>(success ? 1 : 0);

    // 消息
    QByteArray msgBytes = message.toUtf8();
    stream << static_cast<quint16>(msgBytes.size());
    stream.writeRawData(msgBytes.data(), msgBytes.size());

    QByteArray packet = buildPacket(Command::FileReceiveResult, data);
    session->socket->write(packet);

    qDebug() << "FileReceiver: Sent FileReceiveResult - success:" << success
             << "message:" << message;
}

void FileReceiver::sendError(ReceiveSession* session, const QString& errorMessage)
{
    QByteArray data = errorMessage.toUtf8();
    QByteArray packet = buildPacket(Command::Error, data);
    session->socket->write(packet);

    qDebug() << "FileReceiver: Sent Error -" << errorMessage;
}

void FileReceiver::sendInstallResultCommand(ReceiveSession* session, bool success, const QString& message)
{
    if (!session || !session->socket) {
        qWarning() << "FileReceiver: Cannot send install result - invalid session";
        return;
    }

    if (session->socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "FileReceiver: Socket not connected, cannot send install result";
        return;
    }

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 任务ID
    QByteArray taskIdBytes = session->taskId.toUtf8();
    stream << static_cast<quint16>(taskIdBytes.size());
    stream.writeRawData(taskIdBytes.data(), taskIdBytes.size());

    // 设备ID
    QByteArray deviceIdBytes = session->targetDeviceId.toUtf8();
    stream << static_cast<quint16>(deviceIdBytes.size());
    stream.writeRawData(deviceIdBytes.data(), deviceIdBytes.size());

    // 成功标志
    stream << static_cast<quint8>(success ? 1 : 0);

    // 消息
    QByteArray msgBytes = message.toUtf8();
    stream << static_cast<quint16>(msgBytes.size());
    stream.writeRawData(msgBytes.data(), msgBytes.size());

    // 发送 InstallResult 命令
    QByteArray packet = buildPacket(Command::InstallResult, data);
    session->socket->write(packet);

    qDebug() << "FileReceiver: Sent InstallResult - taskId:" << session->taskId
             << "deviceId:" << session->targetDeviceId
             << "success:" << success
             << "message:" << message;
}

// ==================== 会话管理 ====================

FileReceiver::ReceiveSession* FileReceiver::createSession(QTcpSocket* socket)
{
    ReceiveSession* session = new ReceiveSession();
    session->socket = socket;
    session->isActive = false;
    session->receivedBytes = 0;
    session->fileSize = 0;
    session->expectedSha256.clear();
    session->tempFile = nullptr;
    session->hash = nullptr;
    session->timeoutTimer = nullptr;
    return session;
}

void FileReceiver::cleanupSession(ReceiveSession* session)
{
    if (!session) return;

    // 从映射中移除
    if (!session->taskId.isEmpty()) {
        m_taskToSession.remove(session->taskId);
    }

    if (session->tempFile) {
        if (session->tempFile->isOpen()) {
            session->tempFile->close();
        }
        // 删除临时文件（如果接收不完整）
        if (session->receivedBytes < session->fileSize) {
            session->tempFile->remove();
        }
        delete session->tempFile;
        session->tempFile = nullptr;
    }

    if (session->hash) {
        delete session->hash;
        session->hash = nullptr;
    }

    if (session->timeoutTimer) {
        session->timeoutTimer->stop();
        delete session->timeoutTimer;
        session->timeoutTimer = nullptr;
    }

    delete session;
}

FileReceiver::ReceiveSession* FileReceiver::getSessionBySocket(QTcpSocket* socket)
{
    return m_sessions.value(socket, nullptr);
}

// ==================== 辅助方法 ====================

QString FileReceiver::getTempFilePath(const QString& taskId, const QString& fileName)
{
    // 创建临时目录
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/CMC_Receiver/" + taskId;
    QDir dir;
    if (!dir.exists(tempDir)) {
        dir.mkpath(tempDir);
    }

    return tempDir + "/" + fileName;
}

void FileReceiver::notifyInstaller(ReceiveSession* session)
{
    if (!m_installer) {
        qWarning() << "FileReceiver: Installer not set";
        // 直接发送失败结果
        sendFileReceiveResult(session, false, "安装器未初始化");
        return;
    }

    // 调用 Installer 进行安装（异步）
    m_installer->install(session->taskId, session->targetDeviceId, session->tempFile->fileName());

    // 注意：这里不等待安装结果，由 Installer 的信号触发后续


    QString filePath = session->tempFile->fileName();

    qDebug() << "FileReceiver: Notifying installer - taskId:" << session->taskId
             << "targetDevice:" << session->targetDeviceId
             << "filePath:" << filePath;

    emit fileReadyForInstall(session->taskId, session->targetDeviceId, filePath);
}

// ==================== 数据包处理 ====================

bool FileReceiver::parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload)
{
    if (data.size() < PACKET_HEADER_SIZE) {
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 startMark;
    stream >> startMark;

    if (startMark != PACKET_START_MARK) {
        qWarning() << "FileReceiver: Invalid packet start mark:" << startMark;
        return false;
    }

    quint8 cmdByte;
    stream >> cmdByte;
    cmd = static_cast<Command>(cmdByte);

    quint32 payloadSize;
    stream >> payloadSize;

    int totalSize = PACKET_HEADER_SIZE + payloadSize + 2;
    if (data.size() < totalSize) {
        return false;
    }

    payload.resize(payloadSize);
    if (payloadSize > 0) {
        stream.readRawData(payload.data(), payloadSize);
    }

    quint16 receivedChecksum;
    stream >> receivedChecksum;

    // 验证校验和
    quint16 calculatedChecksum = 0;
    for (int i = 0; i < totalSize - 2; ++i) {
        calculatedChecksum ^= static_cast<quint8>(data[i]);
    }

    if (calculatedChecksum != receivedChecksum) {
        qWarning() << "FileReceiver: Checksum mismatch";
        return false;
    }

    return true;
}

QByteArray FileReceiver::buildPacket(Command cmd, const QByteArray& payload)
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

#include "aircraftclient.h"

#include "../domain/aircraftsimulator.h"
#include "../logging/aircraftlogger.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#ifndef AC1001_PROJECT_DIR
#define AC1001_PROJECT_DIR ""
#endif

namespace {

QString ac1001ProjectDir()
{
    const QString projectDir = QString::fromUtf8(AC1001_PROJECT_DIR);
    if (!projectDir.isEmpty() && QFileInfo::exists(QDir(projectDir).filePath("AC-1001.pro"))) {
        return QDir::cleanPath(projectDir);
    }
    return QDir::currentPath();
}

QString sanitizePathPart(QString value, const QString& fallback)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        value = fallback;
    }
    value.replace(QRegularExpression("[\\\\/:*?\"<>|\\s]+"), "_");
    return value;
}

QString readString(QDataStream& stream)
{
    quint16 size = 0;
    stream >> size;
    QByteArray bytes(size, Qt::Uninitialized);
    if (size > 0) {
        stream.readRawData(bytes.data(), size);
    }
    return QString::fromUtf8(bytes);
}

void writeString(QDataStream& stream, const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    stream << static_cast<quint16>(bytes.size());
    stream.writeRawData(bytes.data(), bytes.size());
}

}

AircraftClient::AircraftClient(AircraftSimulator* simulator, QObject *parent)
    : QObject{parent}
    , m_simulator(simulator)
    , m_socket(new QTcpSocket(this))
    , m_lastPort(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &AircraftClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &AircraftClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &AircraftClient::onErrorOccurred);
    connect(m_socket, &QTcpSocket::readyRead, this, &AircraftClient::onReadyRead);
}

void AircraftClient::connectToGroundStation(const QString& host, quint16 port)
{
    m_lastHost = host;
    m_lastPort = port;

    if (isConnected() || m_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }

    AircraftLogger::info(QString("ADG 正在连接地面站 %1:%2").arg(host).arg(port));
    emit connectionChanged(false, QString("ADG 正在连接地面站 %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void AircraftClient::disconnectFromGroundStation()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        AircraftLogger::warn("ADG 当前未连接地面站");
        emit connectionChanged(false, "ADG 未连接地面站");
        return;
    }

    AircraftLogger::info("ADG 主动断开地面站连接");
    m_socket->disconnectFromHost();
}

bool AircraftClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void AircraftClient::onConnected()
{
    AircraftLogger::info("AC-1001 已连接到地面站");
    emit connectionChanged(true, "AC-1001 已连接到地面站");
    sendDeviceStatusFull();
}

void AircraftClient::onDisconnected()
{
    m_receiveBuffer.clear();
    m_transfer = TransferContext();
    AircraftLogger::warn("AC-1001 与地面站断开连接");
    emit connectionChanged(false, "AC-1001 与地面站断开连接");
}

void AircraftClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    AircraftLogger::error(QString("无法连接地面站：%1").arg(m_socket->errorString()));
    emit connectionChanged(false, QString("无法连接地面站：%1").arg(m_socket->errorString()));
}

void AircraftClient::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());

    Command cmd;
    QByteArray payload;
    while (parsePacket(m_receiveBuffer, cmd, payload)) {
        switch (cmd) {
        case Command::FileStart:
            handleFileStart(payload);
            break;
        case Command::FileData:
            handleFileData(payload);
            break;
        case Command::FileEnd:
            handleFileEnd();
            break;
        default:
            break;
        }
    }
}

bool AircraftClient::sendCommand(Command cmd, const QByteArray& payload)
{
    if (!isConnected()) {
        return false;
    }

    const QByteArray packet = buildPacket(cmd, payload);
    return m_socket->write(packet) == packet.size() && m_socket->flush();
}

QByteArray AircraftClient::buildPacket(Command cmd, const QByteArray& payload) const
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

bool AircraftClient::parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload)
{
    if (data.size() < PACKET_HEADER_SIZE) {
        return false;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 startMark;
    stream >> startMark;
    if (startMark != PACKET_START_MARK) {
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
        m_receiveBuffer.clear();
        return false;
    }

    m_receiveBuffer.remove(0, totalSize);
    return true;
}

void AircraftClient::sendDeviceStatusFull()
{
    if (!m_simulator) {
        return;
    }

    const QList<AircraftDevice> devices = m_simulator->devices();

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint16>(devices.size());

    const quint64 now = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
    for (const AircraftDevice& device : devices) {
        writeString(stream, device.deviceId);
        writeString(stream, device.deviceName);
        stream << static_cast<quint8>(device.online ? 1 : 0);
        writeString(stream, device.version);
        stream << now;
    }

    sendCommand(Command::DeviceStatusFull, payload);
    AircraftLogger::info(QString("已向地面站上报 %1 个设备状态").arg(devices.size()));
}

void AircraftClient::sendFileReceiveResult(bool success, const QString& message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    writeString(stream, m_transfer.taskId);
    stream << static_cast<quint8>(success ? 1 : 0);
    writeString(stream, message);

    sendCommand(Command::FileReceiveResult, payload);
}

void AircraftClient::sendInstallResult(bool success, const QString& message)
{
    sendInstallResultForTask(m_transfer.taskId, m_transfer.targetDeviceId, success, message);
}

void AircraftClient::sendInstallResultForTask(const QString& taskId,
                                              const QString& targetDeviceId,
                                              bool success,
                                              const QString& message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    writeString(stream, taskId);
    writeString(stream, targetDeviceId);
    stream << static_cast<quint8>(success ? 1 : 0);
    writeString(stream, message);

    sendCommand(Command::InstallResult, payload);
}

QString AircraftClient::receiveDirectory() const
{
    return QDir::cleanPath(QDir(ac1001ProjectDir()).filePath("data/adg/upgrade_cache"));
}

QString AircraftClient::buildLocalPackagePath(const QString& taskId,
                                              const QString& targetDeviceId,
                                              const QString& fileName) const
{
    const QString taskPart = QString("device_task_%1_%2")
        .arg(sanitizePathPart(taskId, "unknown_task"),
             sanitizePathPart(targetDeviceId, "unknown_device"));
    const QString safeFileName = sanitizePathPart(fileName, "upgrade_package.bin");
    return QDir::cleanPath(QDir(receiveDirectory()).filePath(taskPart + "/" + safeFileName));
}

bool AircraftClient::verifyPackageFile(const QString& filePath,
                                       qint64 expectedSize,
                                       const QString& expectedSha256) const
{
    QFileInfo info(filePath);
    if (!info.exists() || info.size() != expectedSize) {
        return false;
    }

    if (expectedSha256.isEmpty()) {
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }

    const QString actualSha256 = QString::fromLatin1(hash.result().toHex());
    return actualSha256.compare(expectedSha256, Qt::CaseInsensitive) == 0;
}

void AircraftClient::handleFileStart(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    m_transfer = TransferContext();
    m_transfer.taskId = readString(stream);
    m_transfer.targetDeviceId = readString(stream);
    m_transfer.fileName = readString(stream);

    quint64 fileSize = 0;
    stream >> fileSize;
    m_transfer.expectedSize = static_cast<qint64>(fileSize);
    m_transfer.expectedSha256 = readString(stream);
    m_transfer.localPath = buildLocalPackagePath(m_transfer.taskId,
                                                 m_transfer.targetDeviceId,
                                                 m_transfer.fileName);
    m_transfer.tempPath = m_transfer.localPath + ".part";

    if (!m_simulator || !m_simulator->validateTargetDevice(m_transfer.targetDeviceId)) {
        AircraftLogger::warn(QString("目标设备 %1 不在线，拒绝接收升级文件").arg(m_transfer.targetDeviceId));
        sendFileReceiveResult(false, QString("目标设备 %1 不在线").arg(m_transfer.targetDeviceId));
        return;
    }

    QDir dir(QFileInfo(m_transfer.localPath).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        AircraftLogger::error("ADG 无法创建升级文件缓存目录");
        sendFileReceiveResult(false, "ADG 无法创建升级文件缓存目录");
        return;
    }

    QFile tempFile(m_transfer.tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        AircraftLogger::error("ADG 无法创建升级文件缓存");
        sendFileReceiveResult(false, "ADG 无法创建升级文件缓存");
    }

    AircraftLogger::info(QString("开始接收设备 %1 的升级文件 %2")
                         .arg(m_transfer.targetDeviceId, m_transfer.fileName));
}

void AircraftClient::handleFileData(const QByteArray& payload)
{
    if (m_transfer.tempPath.isEmpty()) {
        return;
    }

    QFile tempFile(m_transfer.tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }

    tempFile.write(payload);
    m_transfer.receivedSize += payload.size();
}

void AircraftClient::handleFileEnd()
{
    bool success = m_transfer.receivedSize == m_transfer.expectedSize &&
                   verifyPackageFile(m_transfer.tempPath,
                                     m_transfer.expectedSize,
                                     m_transfer.expectedSha256);

    if (success) {
        if (QFile::exists(m_transfer.localPath)) {
            QFile::remove(m_transfer.localPath);
        }
        QFile tempFile(m_transfer.tempPath);
        success = tempFile.rename(m_transfer.localPath);
    }

    const QString message = success
        ? QString("ADG 已完整接收升级文件：%1").arg(m_transfer.localPath)
        : "ADG 文件接收不完整或校验失败";
    if (success) {
        AircraftLogger::info(message);
    } else {
        AircraftLogger::error(message);
    }
    sendFileReceiveResult(success, message);

    if (!success) {
        sendInstallResult(false, message);
        return;
    }

    const QString taskId = m_transfer.taskId;
    const QString targetDeviceId = m_transfer.targetDeviceId;
    QTimer::singleShot(5000, this, [this, taskId, targetDeviceId]() {
        AircraftLogger::info(QString("设备 %1 模拟安装完成，返回升级成功").arg(targetDeviceId));
        sendInstallResultForTask(taskId,
                                 targetDeviceId,
                                 true,
                                 QString::fromUtf8("AC-1001 模拟安装成功，升级完成"));
    });
}

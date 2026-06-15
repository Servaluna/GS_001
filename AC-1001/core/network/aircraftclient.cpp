#include "aircraftclient.h"

#include "../domain/aircraftsimulator.h"

#include <QDataStream>
#include <QDateTime>

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

    emit connectionChanged(false, QString("ADG 正在连接地面站 %1:%2...").arg(host).arg(port));
    m_socket->connectToHost(host, port);
}

void AircraftClient::disconnectFromGroundStation()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit connectionChanged(false, "ADG 未连接地面站");
        return;
    }

    m_socket->disconnectFromHost();
}

bool AircraftClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void AircraftClient::onConnected()
{
    emit connectionChanged(true, "AC-1001 已连接到地面站");
    sendDeviceStatusFull();
}

void AircraftClient::onDisconnected()
{
    m_receiveBuffer.clear();
    emit connectionChanged(false, "AC-1001 与地面站断开连接");
}

void AircraftClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
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
    const int totalSize = PACKET_HEADER_SIZE + payloadSize + 2;
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
        const QByteArray id = device.deviceId.toUtf8();
        stream << static_cast<quint16>(id.size());
        stream.writeRawData(id.data(), id.size());

        const QByteArray name = device.deviceName.toUtf8();
        stream << static_cast<quint16>(name.size());
        stream.writeRawData(name.data(), name.size());

        stream << static_cast<quint8>(device.online ? 1 : 0);

        const QByteArray version = device.version.toUtf8();
        stream << static_cast<quint16>(version.size());
        stream.writeRawData(version.data(), version.size());

        stream << now;
    }

    sendCommand(Command::DeviceStatusFull, payload);
}

void AircraftClient::sendFileReceiveResult(bool success, const QString& message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    const QByteArray taskId = m_transfer.taskId.toUtf8();
    stream << static_cast<quint16>(taskId.size());
    stream.writeRawData(taskId.data(), taskId.size());

    stream << static_cast<quint8>(success ? 1 : 0);

    const QByteArray msg = message.toUtf8();
    stream << static_cast<quint16>(msg.size());
    stream.writeRawData(msg.data(), msg.size());

    sendCommand(Command::FileReceiveResult, payload);
}

void AircraftClient::sendInstallResult(bool success, const QString& message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    const QByteArray taskId = m_transfer.taskId.toUtf8();
    stream << static_cast<quint16>(taskId.size());
    stream.writeRawData(taskId.data(), taskId.size());

    const QByteArray deviceId = m_transfer.targetDeviceId.toUtf8();
    stream << static_cast<quint16>(deviceId.size());
    stream.writeRawData(deviceId.data(), deviceId.size());

    stream << static_cast<quint8>(success ? 1 : 0);

    const QByteArray msg = message.toUtf8();
    stream << static_cast<quint16>(msg.size());
    stream.writeRawData(msg.data(), msg.size());

    sendCommand(Command::InstallResult, payload);
}

void AircraftClient::handleFileStart(const QByteArray& payload)
{
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::BigEndian);

    auto readString = [&stream]() {
        quint16 size = 0;
        stream >> size;
        QByteArray bytes(size, Qt::Uninitialized);
        if (size > 0) {
            stream.readRawData(bytes.data(), size);
        }
        return QString::fromUtf8(bytes);
    };

    m_transfer = TransferContext();
    m_transfer.taskId = readString();
    m_transfer.targetDeviceId = readString();
    m_transfer.fileName = readString();

    quint64 fileSize = 0;
    stream >> fileSize;
    m_transfer.expectedSize = static_cast<qint64>(fileSize);
    m_transfer.expectedSha256 = readString();

    if (!m_simulator || !m_simulator->validateTargetDevice(m_transfer.targetDeviceId)) {
        sendFileReceiveResult(false, QString("目标设备 %1 不在线").arg(m_transfer.targetDeviceId));
        return;
    }

    sendFileReceiveResult(true, "AC-1001 已准备接收文件");
}

void AircraftClient::handleFileData(const QByteArray& payload)
{
    m_transfer.data.append(payload);
    m_transfer.receivedSize += payload.size();
}

void AircraftClient::handleFileEnd()
{
    const bool success = m_simulator && m_simulator->verifyPackage(m_transfer.data,
                                                                   m_transfer.expectedSize,
                                                                   m_transfer.expectedSha256);
    const QString message = success ? "AC-1001 文件接收完成" : "AC-1001 文件校验失败";

    sendFileReceiveResult(success, message);
    sendInstallResult(success, success ? "AC-1001 模拟安装完成" : message);
}

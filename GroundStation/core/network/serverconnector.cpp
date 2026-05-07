#include "serverconnector.h"

ServerConnector::ServerConnector(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &ServerConnector::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ServerConnector::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ServerConnector::onErrorOccurred);
    connect(m_socket, &QTcpSocket::readyRead, this, &ServerConnector::onReadyRead);
}

ServerConnector::~ServerConnector()
{
    cleanupSocket();
}

ServerConnector& ServerConnector::instance()
{
    static ServerConnector instance;
    return instance;
}

bool ServerConnector::connectToServer(const QString& host, quint16 port)
{
    if (isConnected() && m_host == host && m_port == port) {
        DEBUG_LOCATION << "已经连接到服务器" << host << ":" << port;
        return true;
    }

    if (isConnected()) {
        DEBUG_LOCATION << "断开当前连接，准备连接到新服务器";
        m_pendingHost = host;
        m_pendingPort = port;
        m_pendingConnection = true;
        disconnectFromServer();
        return true;
    }

    doConnect(host, port);
    return true;
}

void ServerConnector::disconnectFromServer()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

void ServerConnector::cleanupSocket()
{
    if (!m_socket) {
        return;
    }

    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ClosingState) {
        m_socket->waitForDisconnected(2000);
    }

    m_socket->close();
    m_socket->deleteLater();
    m_socket = nullptr;
    DEBUG_LOCATION << "socket 清理完成";
}

void ServerConnector::doConnect(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_socket->connectToHost(host, port);
}

void ServerConnector::onConnected()
{
    DEBUG_LOCATION << "客户端已连接到服务器" << m_host << ":" << m_port << "socket:" << m_socket;
    emit connected();
}

void ServerConnector::onDisconnected()
{
    DEBUG_LOCATION << "与服务器断开连接";
    emit disconnected();

    if (m_pendingConnection) {
        m_pendingConnection = false;
        doConnect(m_pendingHost, m_pendingPort);
    }
}

void ServerConnector::onErrorOccurred(QAbstractSocket::SocketError error)
{
    const QString errorMsg = m_socket ? m_socket->errorString() : QString("Socket 不存在");
    qCritical() << "网络错误状态码:" << error;
    qCritical() << "网络错误信息:" << errorMsg;
    emit errorOccurred(errorMsg);
}

void ServerConnector::onReadyRead()
{
    while (true) {
        Message msg = receiveMessage(m_socket, m_buffer, m_expectedLength);
        if (!msg.isValid()) {
            break;
        }

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
        DEBUG_LOCATION << "Session" << m_socket << "lastActiveTime:" << m_lastActiveTime;

        switch (msg.type) {
        case MessageType::LoginResponse:
            handleLoginResponse(msg);
            DEBUG_LOCATION << "消息类型:" << msg.type;
            break;

        case MessageType::TaskFileInfo:
            emit fileInfoReceived(msg.data["task_uuid"].toString(msg.messageId),
                                  msg.data["total_size"].toInteger(),
                                  msg.data["sha256"].toString());
            break;

        case MessageType::FileData: {
            const QByteArray chunkData = QByteArray::fromBase64(msg.data["chunk_data"].toString().toUtf8());
            emit fileChunkReceived(msg.data["task_uuid"].toString(msg.messageId),
                                   chunkData,
                                   msg.data["chunk_index"].toInt(),
                                   msg.data["is_last"].toBool());
            break;
        }

        case MessageType::Error: {
            QString errorMsg = msg.data["error"].toString();
            if (errorMsg.isEmpty()) {
                errorMsg = "Unknown server error";
            }
            emit errorOccurred(errorMsg);
            break;
        }

        default:
            DEBUG_LOCATION << "未处理的消息类型:" << msg.type;
            break;
        }
    }
}

void ServerConnector::loginRequest(const QString& username, const QString& password)
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return;
    }

    QJsonObject data;
    data["username"] = username;
    data["password"] = password;

    Message reqMsg;
    reqMsg.type = MessageType::LoginRequest;
    reqMsg.data = data;

    sendMessage(m_socket, reqMsg);

    DEBUG_LOCATION << "发送登录请求:" << username
                   << "reqMsg.type:" << reqMsg.type
                   << "reqMsg.data:" << reqMsg.data;
}

bool ServerConnector::fileDownloadRequest(const QString& fileCode, qint64 offset, const QString& taskUuid)
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return false;
    }

    QJsonObject data;
    data["file_code"] = fileCode;
    data["fileId"] = fileCode;
    data["offset"] = offset;
    data["task_uuid"] = taskUuid;

    Message reqMsg(MessageType::GetTaskFile, data);
    return sendMessage(m_socket, reqMsg);
}

void ServerConnector::handleLoginResponse(const Message& respMsg)
{
    const QString token = respMsg.data["token"].toString();
    const UserInfo user = UserInfo::fromJson(respMsg.data["user"].toObject());

    emit loginSuccess(token, user);
}

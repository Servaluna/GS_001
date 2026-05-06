#include "serverconnector.h"

ServerConnector::ServerConnector(QObject *parent)
    : QObject(parent)
    ,m_socket(nullptr)
{
    m_socket = new QTcpSocket(this);

    // 连接信号槽
    connect(m_socket, &QTcpSocket::connected, this, &ServerConnector::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ServerConnector::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ServerConnector::onErrorOccurred);
    connect(m_socket, &QTcpSocket::readyRead, this, &ServerConnector::onReadyRead);
}

ServerConnector::~ServerConnector()
{
    cleanupSocket();
}

void ServerConnector::cleanupSocket()
{
    if (m_socket) {
        if (m_socket->isOpen()) {
            m_socket->disconnectFromHost();
        }

        m_socket->blockSignals(true);

        m_socket->deleteLater();
        if (m_socket->state() == QAbstractSocket::ConnectedState ||
            m_socket->state() == QAbstractSocket::ClosingState) {
            m_socket->waitForDisconnected(2000);
        }

        m_socket->blockSignals(false);

        m_socket->close();
        m_socket = nullptr;
        DEBUG_LOCATION << "socket 清理完成";
    }
}

ServerConnector& ServerConnector::instance()
{
    static ServerConnector instance;
    return instance;
}

bool ServerConnector::connectToServer(const QString& host, quint16 port)
{
    // 如果已经连接到同一个服务器，直接返回成功
    if (isConnected() && m_host == host && m_port == port) {
        DEBUG_LOCATION << "已经连接到服务器" << host << ":" << port;
        return true;
    }

    // 如果已连接到其他服务器，先断开
    if (isConnected()) {
        DEBUG_LOCATION << "断开当前连接，准备连接到新服务器";
        m_pendingHost = host;
        m_pendingPort = port;
        m_pendingConnection = true;
        disconnectFromServer();     // 注意：disconnect() 应该是同步的，或者等待它完成
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

void ServerConnector::doConnect(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_socket->connectToHost(host, port);
}

void ServerConnector::onConnected()
{
    DEBUG_LOCATION << "客户端已连接到服务器" << m_host << ":" << m_port << "，会话：" << m_socket;
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
    QString errorMsg = m_socket->errorString();
    qCritical() << "网络错误状态码:" << error;
    qCritical() << "网络错误信息:" << errorMsg;
    emit errorOccurred(errorMsg);
}

void ServerConnector::onReadyRead()
{
        // 循环处理所有（可能是多条）完整消息
        while (true) {
            Message msg = receiveMessage(m_socket, m_buffer, m_expectedLength);

            if (!msg.isValid()) {
                break;  // 没有完整消息，等待更多数据
            }

            // 更新活跃时间
            m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
            DEBUG_LOCATION << "Session" << m_socket << "lastActiveTime:" << m_lastActiveTime;

            // 选择对应的处理函数
            switch (msg.type) {
            case MessageType::LoginResponse:
                handleLoginResponse(msg);
                DEBUG_LOCATION << "消息类型:" << msg.type;
                break;

            case MessageType::Error: {
                QString errorMsg = msg.data["error"].toString();
                if (errorMsg.isEmpty()) errorMsg = "Unknown server error";
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

    DEBUG_LOCATION << "发送登录请求" << username
                   << "reqMsg.type: " << reqMsg.type
                   << "reqMsg.data: " << reqMsg.data;
}

bool ServerConnector::fileDownloadRequest(QString fileId, qint64 offset)
{
    if (!isConnected()) {
        emit errorOccurred("Not connected to server");
        return false;
    }

    QJsonObject data;
    data["fileId"] = fileId;
    data["offset"] = offset;

    Message reqMsg(MessageType::GetTaskFile, data);
    return sendMessage(m_socket, reqMsg);
}

void ServerConnector::handleLoginResponse(const Message& respMsg)
{
    QString token = respMsg.data["token"].toString();
    UserInfo user =UserInfo::fromJson(respMsg.data["user"].toObject());

    emit loginSuccess(token, user);
}

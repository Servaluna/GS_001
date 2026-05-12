#include "server.h"

#define DEFAULT_PORT 8000

Server::Server(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
}

Server::~Server()
{
    stop();
}

bool Server::start(quint16 port)
{
    if (m_isListening) {
        return true;
    }

    const quint16 listenPort = port == 0 ? DEFAULT_PORT : port;
    if (!m_server->listen(QHostAddress::Any, listenPort)) {
        m_lastError = m_server->errorString();
        emit startFailed(m_lastError);
        emit logMessage(QString("服务器启动失败: %1").arg(m_lastError));
        return false;
    }

    m_isListening = true;
    m_listenPort = listenPort;
    emit started(listenPort);
    emit logMessage(QString("服务器启动成功，监听端口: %1").arg(listenPort));
    return true;
}

void Server::stop()
{
    if (!m_isListening && !m_server->isListening()) {
        return;
    }

    for (QTcpSocket* socket : m_clients.keys()) {
        ClientInfo& info = m_clients[socket];
        if (info.isDisconnected) {
            continue;
        }

        emit logMessage(QString("断开客户端 %1:%2").arg(info.address).arg(info.port));
        info.isDisconnected = true;
        info.isLoggedIn = false;
        info.handler = nullptr;

        socket->disconnectFromHost();
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->waitForDisconnected(1000);
        }
        socket->deleteLater();
    }

    m_server->close();
    m_isListening = false;
    m_listenPort = 0;

    emit clientListChanged();
    emit stopped();
    emit logMessage("服务器已停止");
}

bool Server::isListening() const
{
    return m_isListening;
}

quint16 Server::listenPort() const
{
    return m_listenPort;
}

int Server::activeClientCount() const
{
    int count = 0;
    for (const ClientInfo& info : m_clients) {
        if (!info.isDisconnected) {
            ++count;
        }
    }
    return count;
}

QList<ClientInfo> Server::clients() const
{
    return m_clients.values();
}

QString Server::lastError() const
{
    return m_lastError;
}

void Server::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);

        ClientHandler* handler = new ClientHandler(socket, this);

        ClientInfo info(socket);
        info.handler = handler;
        m_clients[socket] = info;

        connect(handler, &ClientHandler::logMessage, this, &Server::onClientLog);
        connect(handler, &ClientHandler::loginSucceeded,
                this, &Server::onClientLoginSucceeded);
        connect(handler, &ClientHandler::finished,
                this, [this, handler]() { onClientFinished(handler); });

        emit logMessage(QString("新客户端连接: %1:%2").arg(info.address).arg(info.port));
    }

    emit clientListChanged();
}

void Server::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    markSocketDisconnected(socket);
    socket->deleteLater();
    emit clientListChanged();
}

void Server::onClientLog(const QString& msg)
{
    emit logMessage(QString("[ClientHandler] %1").arg(msg));
}

void Server::onClientFinished(ClientHandler* handler)
{
    QTcpSocket* socket = socketForHandler(handler);
    if (!socket) {
        return;
    }

    markSocketDisconnected(socket);
    emit clientListChanged();
}

void Server::onClientLoginSucceeded(QString username, QString role, QString token)
{
    ClientHandler* handler = qobject_cast<ClientHandler*>(sender());
    if (!handler) {
        return;
    }

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        ClientInfo& info = it.value();
        if (info.handler == handler) {
            info.isLoggedIn = true;
            info.isDisconnected = false;
            info.username = username;
            info.role = role;
            info.token = token;
            info.loginTime = QDateTime::currentDateTime();
            emit logMessage(QString("客户端登录成功: %1 (%2)").arg(username, role));
            emit clientListChanged();
            return;
        }
    }
}

void Server::markSocketDisconnected(QTcpSocket* socket)
{
    if (!m_clients.contains(socket)) {
        return;
    }

    ClientInfo& info = m_clients[socket];
    if (info.isDisconnected) {
        return;
    }

    info.isDisconnected = true;
    info.isLoggedIn = false;
    info.handler = nullptr;

    const qint64 onlineSeconds = info.connectTime.secsTo(QDateTime::currentDateTime());
    emit logMessage(QString("客户端断开: %1:%2，在线时长: %3 秒")
                        .arg(info.address)
                        .arg(info.port)
                        .arg(onlineSeconds));
}

QTcpSocket* Server::socketForHandler(ClientHandler* handler) const
{
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        if (it.value().handler == handler) {
            return it.key();
        }
    }
    return nullptr;
}

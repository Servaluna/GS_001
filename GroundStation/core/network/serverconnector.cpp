#include "serverconnector.h"

#include "../logging/logger.h"

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
        Logger::debug("SERVER_ALREADY_CONNECTED",
                      QString("已经连接到服务器 %1:%2").arg(host).arg(port),
                      {{"host", host}, {"port", port}});
        return true;
    }

    if (isConnected()) {
        Logger::info("SERVER_RECONNECT",
                     "断开当前服务器连接，准备连接到新服务器",
                     {{"old_host", m_host}, {"old_port", m_port}, {"new_host", host}, {"new_port", port}});
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
    Logger::debug("SERVER_SOCKET_CLEANED", "服务器连接 socket 已清理");
}

void ServerConnector::doConnect(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_socket->connectToHost(host, port);
}

void ServerConnector::onConnected()
{
    Logger::info("SERVER_CONNECTED",
                 QString("客户端已连接到服务器 %1:%2").arg(m_host).arg(m_port),
                 {{"host", m_host}, {"port", m_port}});
    emit connected();
}

void ServerConnector::onDisconnected()
{
    Logger::warn("SERVER_DISCONNECTED",
                 QString("与服务器 %1:%2 断开连接").arg(m_host).arg(m_port),
                 {{"host", m_host}, {"port", m_port}});
    emit disconnected();

    if (m_pendingConnection) {
        m_pendingConnection = false;
        doConnect(m_pendingHost, m_pendingPort);
    }
}

void ServerConnector::onErrorOccurred(QAbstractSocket::SocketError error)
{
    const QString errorMsg = m_socket ? m_socket->errorString() : QString("Socket 不存在");
    Logger::error("SERVER_NETWORK_ERROR",
                  "服务器连接发生网络错误",
                  {{"host", m_host}, {"port", m_port}, {"error_code", static_cast<int>(error)}, {"error_message", errorMsg}});
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

        switch (msg.type) {
        case MessageType::LoginResponse:
            handleLoginResponse(msg);
            break;

        case MessageType::TaskFileInfo:
            emit fileInfoReceived(msg.data["task_uuid"].toString(msg.messageId),
                                  msg.data["total_size"].toInteger(),
                                  msg.data["sha256"].toString());
            break;

        case MessageType::CurrentUserTasksList:
            Logger::info("TASK_SYNC_RESPONSE_RECEIVED",
                         "收到服务器任务同步响应",
                         {{"aircraft_task_count", msg.data["aircraft_tasks"].toArray().size()},
                          {"device_task_count", msg.data["device_tasks"].toArray().size()}});
            emit currentUserTasksReceived(msg.data["aircraft_tasks"].toArray(),
                                          msg.data["device_tasks"].toArray());
            break;

        case MessageType::TaskStatusUpdated:
            if (!msg.data["success"].toBool()) {
                Logger::warn("TASK_STATUS_UPDATE_RESPONSE",
                             "任务状态回写失败",
                             {{"aircraft_task_id", msg.data["aircraft_task_id"].toInt(-1)},
                              {"device_task_id", msg.data["device_task_id"].toInt(-1)},
                              {"message", msg.data["message"].toString()}});
            } else {
                Logger::info("TASK_STATUS_UPDATE_RESPONSE",
                             "任务状态回写成功",
                             {{"aircraft_task_id", msg.data["aircraft_task_id"].toInt(-1)},
                              {"device_task_id", msg.data["device_task_id"].toInt(-1)}});
            }
            emit taskStatusUpdated(msg.data["success"].toBool(),
                                   msg.data["aircraft_task_id"].toInt(-1),
                                   msg.data["device_task_id"].toInt(-1),
                                   msg.data["message"].toString());
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
            Logger::error("SERVER_MESSAGE_ERROR", errorMsg);
            emit errorOccurred(errorMsg);
            break;
        }

        default:
            Logger::debug("SERVER_MESSAGE_IGNORED",
                          "收到未处理的服务器消息类型",
                          {{"message_type", static_cast<int>(msg.type)}});
            break;
        }
    }
}

void ServerConnector::loginRequest(const QString& username, const QString& password)
{
    if (!isConnected()) {
        Logger::warn("AUTH_LOGIN_REQUEST_FAILED", "未连接服务器，无法发送登录请求", {{"username", username}});
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject data;
    data["username"] = username;
    data["password"] = password;

    Message reqMsg;
    reqMsg.type = MessageType::LoginRequest;
    reqMsg.data = data;

    if (sendMessage(m_socket, reqMsg)) {
        Logger::info("AUTH_LOGIN_REQUEST", "发送登录请求", {{"username", username}});
    } else {
        Logger::error("AUTH_LOGIN_REQUEST_FAILED", "登录请求发送失败", {{"username", username}});
        emit errorOccurred("登录请求发送失败");
    }
}

bool ServerConnector::logoutRequest()
{
    if (!isConnected()) {
        Logger::warn("AUTH_LOGOUT_REQUEST_FAILED", "未连接服务器，无法发送退出登录请求");
        return false;
    }

    Message reqMsg;
    reqMsg.type = MessageType::Logout;

    const bool ok = sendMessage(m_socket, reqMsg);
    if (ok) {
        m_socket->waitForBytesWritten(1000);
        Logger::info("AUTH_LOGOUT_REQUEST", "发送退出登录请求");
    } else {
        Logger::error("AUTH_LOGOUT_REQUEST_FAILED", "退出登录请求发送失败");
    }
    return ok;
}

bool ServerConnector::requestCurrentUserTasks(int userId, int roleId)
{
    if (!isConnected()) {
        Logger::warn("TASK_SYNC_REQUEST_FAILED",
                     "未连接服务器，无法同步任务",
                     {{"user_id", userId}, {"role_id", roleId}});
        emit errorOccurred("未连接到服务器");
        return false;
    }

    QJsonObject data;
    data["user_id"] = userId;
    data["role_id"] = roleId;

    Message reqMsg(MessageType::GetCurrentUserTasks, data);
    const bool ok = sendMessage(m_socket, reqMsg);
    if (ok) {
        Logger::info("TASK_SYNC_REQUEST",
                     "发送任务同步请求",
                     {{"user_id", userId}, {"role_id", roleId}});
    } else {
        Logger::error("TASK_SYNC_REQUEST_FAILED",
                      "任务同步请求发送失败",
                      {{"user_id", userId}, {"role_id", roleId}});
        emit errorOccurred("任务同步请求发送失败");
    }
    return ok;
}

bool ServerConnector::updateTaskStatus(const QJsonObject& statusData)
{
    if (!isConnected()) {
        Logger::warn("TASK_STATUS_UPDATE_REQUEST_FAILED",
                     "未连接服务器，无法回写任务状态",
                     statusData);
        return false;
    }

    Message reqMsg(MessageType::UpdateTaskStatus, statusData);
    const bool ok = sendMessage(m_socket, reqMsg);
    if (ok) {
        Logger::info("TASK_STATUS_UPDATE_REQUEST",
                     "发送任务状态回写请求",
                     statusData);
    } else {
        Logger::error("TASK_STATUS_UPDATE_REQUEST_FAILED",
                      "任务状态回写请求发送失败",
                      statusData);
    }
    return ok;
}

bool ServerConnector::fileDownloadRequest(const QString& fileCode, qint64 offset, const QString& taskUuid)
{
    if (!isConnected()) {
        Logger::warn("DOWNLOAD_REQUEST_FAILED",
                     "未连接服务器，无法请求下载文件",
                     {{"file_code", fileCode}, {"task_uuid", taskUuid}, {"offset", offset}});
        emit errorOccurred("未连接到服务器");
        return false;
    }

    QJsonObject data;
    data["file_code"] = fileCode;
    data["fileId"] = fileCode;
    data["offset"] = offset;
    data["task_uuid"] = taskUuid;

    Message reqMsg(MessageType::GetTaskFile, data);
    const bool ok = sendMessage(m_socket, reqMsg);
    if (ok) {
        Logger::info("DOWNLOAD_REQUEST",
                     "发送文件下载请求",
                     {{"file_code", fileCode}, {"task_uuid", taskUuid}, {"offset", offset}});
    } else {
        Logger::error("DOWNLOAD_REQUEST_FAILED",
                      "文件下载请求发送失败",
                      {{"file_code", fileCode}, {"task_uuid", taskUuid}, {"offset", offset}});
    }
    return ok;
}

void ServerConnector::handleLoginResponse(const Message& respMsg)
{
    const QString token = respMsg.data["token"].toString();
    const UserInfo user = UserInfo::fromJson(respMsg.data["user"].toObject());

    Logger::info("AUTH_LOGIN_RESPONSE",
                 "收到登录成功响应",
                 {{"username", user.username}, {"role", user.role}, {"user_id", user.user_id}});
    emit loginSuccess(token, user);
}

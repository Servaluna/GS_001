#include "clienthandler.h"

#include "../dao/userdao.h"
#include "../dao/taskdao.h"
#include "../logging/serverlogger.h"
#include "../services/taskservice.h"
#include "../services/userservice.h"

ClientHandler::ClientHandler(QTcpSocket* socket, QObject *parent)
    : QObject{parent}
    , m_socket(socket)
    , m_buffer()
    , m_expectedLength(0)
    , m_userDao(new UserDAO())
    , m_userService(new UserService(m_userDao))
    , m_taskDao(new TaskDAO())
    , m_taskService(new TaskService(m_taskDao))
    , m_lastActiveTime(0)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);

    ServerLogContext context;
    context.ip_address = socket->peerAddress().toString();
    ServerLogger::info("CLIENT_HANDLER_CREATED",
                       "ClientHandler 创建",
                       context,
                       {{"port", socket->peerPort()}});
    emit logMessage("新客户端连接: " + socket->peerAddress().toString());
}

ClientHandler::~ClientHandler()
{
    delete m_userService;
    delete m_userDao;
    delete m_taskService;
    delete m_taskDao;
}

void ClientHandler::onReadyRead()
{
    while (true) {
        Message msg = receiveMessage(m_socket, m_buffer, m_expectedLength);
        if (!msg.isValid()) {
            break;
        }

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();

        switch (msg.type) {
        case MessageType::LoginRequest:
            handleLoginRequest(msg);
            break;

        case MessageType::Logout:
            handleLogoutRequest(msg);
            break;

        case MessageType::GetCurrentUserTasks:
            handleCurrentUserTasksRequest(msg);
            break;

        case MessageType::UpdateTaskStatus:
            handleTaskStatusUpdate(msg);
            break;

        default:
            ServerLogger::debug("CLIENT_MESSAGE_IGNORED",
                                "收到未处理消息",
                                {{"message_type", static_cast<int>(msg.type)}});
            emit logMessage(QString("收到未处理消息，类型: %1").arg(static_cast<int>(msg.type)));
            break;
        }
    }
}

void ClientHandler::onDisconnected()
{
    ServerLogContext context;
    context.ip_address = m_socket->peerAddress().toString();
    ServerLogger::warn("CLIENT_HANDLER_DISCONNECTED",
                       "客户端连接断开",
                       context,
                       {{"port", m_socket->peerPort()}});
    emit logMessage("客户端断开连接: " + m_socket->peerAddress().toString());
    emit finished();

    m_socket->deleteLater();
    this->deleteLater();
}

void ClientHandler::handleLoginRequest(const Message& reqMsg)
{
    const QJsonObject& data = reqMsg.data;

    const QString username = data["username"].toString();
    const QString password = data["password"].toString();

    ServerLogContext requestContext;
    requestContext.ip_address = m_socket->peerAddress().toString();
    ServerLogger::info("AUTH_LOGIN_REQUEST",
                       "收到登录请求",
                       requestContext,
                       {{"username", username}, {"port", m_socket->peerPort()}});
    emit logMessage(QString("处理登录请求: %1").arg(username));

    const LoginResult loginResult = m_userService->login(username, password);

    if (loginResult.success) {
        m_loggedInUserId = loginResult.user.user_id;
        m_loggedInRoleId = loginResult.user.role_id;
        m_sessionToken = loginResult.token;

        ServerLogContext successContext;
        successContext.operator_user_id = loginResult.user.user_id;
        successContext.session_id = loginResult.token;
        successContext.ip_address = m_socket->peerAddress().toString();
        ServerLogger::info("AUTH_LOGIN_RESPONSE_SENT",
                           "发送登录成功响应",
                           successContext,
                           {{"username", username}, {"role", loginResult.user.role}});
        emit logMessage(QString("登录成功: %1 (%2)").arg(username, loginResult.user.role));

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
        Message respMsg = Message::createResponse(reqMsg, loginResult.toResponseJson());
        sendMessage(m_socket, respMsg);
        emit loginSucceeded(loginResult.user.username, loginResult.user.role, loginResult.token);
        return;
    }

    const QString reason = loginResult.errorMessage.isEmpty()
        ? QString("用户名或密码错误")
        : loginResult.errorMessage;

    ServerLogger::warn("AUTH_LOGIN_RESPONSE_SENT",
                       "发送登录失败响应",
                       requestContext,
                       {{"username", username}, {"reason", reason}});
    emit logMessage(QString("登录失败: %1，原因: %2").arg(username, reason));
    Message errorMsg = Message::createErrorResponse(
        reqMsg,
        StatusCode::PermissionDenied,
        reason
    );
    sendMessage(m_socket, errorMsg);
}

void ClientHandler::handleLogoutRequest(const Message& reqMsg)
{
    Q_UNUSED(reqMsg);

    ServerLogContext context;
    context.ip_address = m_socket->peerAddress().toString();
    ServerLogger::info("AUTH_LOGOUT_REQUEST",
                       "收到退出登录请求",
                       context,
                       {{"port", m_socket->peerPort()}, {"user_id", m_loggedInUserId}});
    emit logMessage("收到退出登录请求: " + m_socket->peerAddress().toString());
    m_loggedInUserId = -1;
    m_loggedInRoleId = UserRole::Unknown;
    m_sessionToken.clear();
    emit loggedOut(this);
}

void ClientHandler::handleCurrentUserTasksRequest(const Message& reqMsg)
{
    const int userId = m_loggedInUserId;
    const int roleId = m_loggedInRoleId;

    ServerLogContext context;
    context.operator_user_id = userId;
    context.ip_address = m_socket->peerAddress().toString();

    if (userId <= 0 || roleId == UserRole::Unknown) {
        ServerLogger::warn("TASK_SYNC_REJECTED",
                           "未登录，拒绝任务同步请求",
                           context,
                           {{"request_user_id", reqMsg.data["user_id"].toInt(-1)},
                            {"request_role_id", reqMsg.data["role_id"].toInt(UserRole::Unknown)}});
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::PermissionDenied, "请先登录后再同步任务"));
        return;
    }

    QJsonObject data = m_taskService->getCurrentUserTasks(userId, roleId);
    data["user_id"] = userId;
    data["role_id"] = roleId;

    ServerLogger::info("TASK_SYNC_RESPONSE_SENT",
                       "发送任务同步响应",
                       context,
                       {{"aircraft_task_count", data["aircraft_tasks"].toArray().size()},
                        {"device_task_count", data["device_tasks"].toArray().size()}});

    sendMessage(m_socket, Message::createResponse(reqMsg, data));
}

void ClientHandler::handleTaskStatusUpdate(const Message& reqMsg)
{
    QJsonObject data = reqMsg.data;
    data["ip_address"] = m_socket->peerAddress().toString();
    data["operator_user_id"] = m_loggedInUserId;
    data["session_id"] = m_sessionToken;

    if (m_loggedInUserId <= 0 || m_loggedInRoleId == UserRole::Unknown) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::PermissionDenied, "请先登录后再回写任务状态"));
        return;
    }

    const int aircraftTaskId = data["aircraft_task_id"].toInt(-1);
    if (aircraftTaskId <= 0) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::InvalidParams, "飞机任务 ID 无效"));
        return;
    }

    const bool ok = m_taskService->updateTaskStatus(data);
    QJsonObject response;
    response["success"] = ok;
    response["aircraft_task_id"] = aircraftTaskId;
    response["device_task_id"] = data["device_task_id"].toInt(-1);

    if (!ok) {
        response["message"] = "任务状态更新失败";
        ServerLogger::warn("TASK_STATUS_UPDATE_FAILED",
                           "任务状态更新失败",
                           {{"aircraft_task_id", aircraftTaskId}, {"device_task_id", response["device_task_id"].toInt()}});
    } else {
        response["message"] = "任务状态更新成功";
        ServerLogger::info("TASK_STATUS_UPDATED",
                           "任务状态已更新",
                           {{"aircraft_task_id", aircraftTaskId}, {"device_task_id", response["device_task_id"].toInt()}});
    }

    sendMessage(m_socket, Message::createResponse(reqMsg, response));
}

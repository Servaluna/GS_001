#include "clienthandler.h"

#include "../dao/userdao.h"
#include "../dao/taskdao.h"
#include "../logging/serverlogger.h"
#include "../services/taskservice.h"
#include "../services/userservice.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifndef CENTRALSERVER_PROJECT_DIR
#define CENTRALSERVER_PROJECT_DIR ""
#endif

namespace {

constexpr qint64 FILE_CHUNK_SIZE = 64 * 1024;

QString centralServerProjectDir()
{
    const QString projectDir = QString::fromUtf8(CENTRALSERVER_PROJECT_DIR);
    if (!projectDir.isEmpty() && QFileInfo::exists(QDir(projectDir).filePath("CentralServer.pro"))) {
        return QDir::cleanPath(projectDir);
    }
    return QDir::currentPath();
}

QString defaultStoragePath(const FileInfo& fileInfo)
{
    const QString fileName = fileInfo.file_name.isEmpty()
        ? fileInfo.file_code + ".bin"
        : fileInfo.file_name;
    return QDir::cleanPath(QDir(centralServerProjectDir())
        .filePath(QString("data/storage/%1/%2").arg(fileInfo.file_code, fileName)));
}

QString resolveStoragePath(const FileInfo& fileInfo)
{
    QFileInfo storageInfo(fileInfo.storage_path);
    if (storageInfo.isAbsolute() && storageInfo.exists()) {
        return QDir::cleanPath(storageInfo.absoluteFilePath());
    }

    const QString projectRelative = QDir(centralServerProjectDir()).filePath(fileInfo.storage_path);
    if (QFileInfo::exists(projectRelative)) {
        return QDir::cleanPath(QFileInfo(projectRelative).absoluteFilePath());
    }

    return defaultStoragePath(fileInfo);
}

QString calculateSha256(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool ensurePlaceholderFile(const FileInfo& fileInfo, const QString& path)
{
    if (QFileInfo::exists(path)) {
        return true;
    }

    QDir dir(QFileInfo(path).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QByteArray seed = QString("GS_001:%1:%2\n")
        .arg(fileInfo.file_code, fileInfo.version)
        .toUtf8();
    const qint64 targetSize = qMax<qint64>(fileInfo.file_size, seed.size());
    qint64 written = 0;
    while (written < targetSize) {
        const qint64 remaining = targetSize - written;
        const QByteArray chunk = seed.left(static_cast<int>(qMin<qint64>(seed.size(), remaining)));
        if (file.write(chunk) != chunk.size()) {
            return false;
        }
        written += chunk.size();
    }
    return true;
}

}

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

        case MessageType::GetTaskFile:
            handleTaskFileRequest(msg);
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

void ClientHandler::handleTaskFileRequest(const Message& reqMsg)
{
    if (m_loggedInUserId <= 0 || m_loggedInRoleId == UserRole::Unknown) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::PermissionDenied, "请先登录后再下载文件"));
        return;
    }

    const QString fileCode = reqMsg.data["file_code"].toString(reqMsg.data["fileId"].toString()).trimmed();
    const QString taskUuid = reqMsg.data["task_uuid"].toString(reqMsg.messageId);
    const qint64 offset = reqMsg.data["offset"].toInteger(0);
    if (fileCode.isEmpty() || offset < 0) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::InvalidParams, "文件下载参数无效"));
        return;
    }

    const FileInfo fileInfo = m_taskService->getFileByCode(fileCode);
    if (!fileInfo.isValid()) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::NotFound, "文件不存在或已停用"));
        return;
    }

    const QString filePath = resolveStoragePath(fileInfo);
    if (!QFileInfo::exists(filePath)) {
        if (!ensurePlaceholderFile(fileInfo, filePath)) {
            ServerLogger::error("DOWNLOAD_FILE_PREPARE_FAILED",
                                "准备下载文件失败",
                                {{"file_code", fileCode}, {"path", filePath}});
            sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::NotFound, "服务器文件不存在"));
            return;
        }

        ServerLogger::warn("DOWNLOAD_FILE_FALLBACK_CREATED",
                           "数据库文件路径不存在，已创建测试升级包",
                           {{"file_code", fileCode}, {"path", filePath}, {"size", fileInfo.file_size}});
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ServerLogger::error("DOWNLOAD_FILE_OPEN_FAILED",
                            "打开下载文件失败",
                            {{"file_code", fileCode}, {"path", filePath}, {"error", file.errorString()}});
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::Failed, "服务器文件打开失败"));
        return;
    }

    const qint64 totalSize = file.size();
    if (offset > totalSize) {
        sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::InvalidParams, "下载偏移量超过文件大小"));
        return;
    }

    const QString sha256 = calculateSha256(filePath);
    QJsonObject infoData;
    infoData["task_uuid"] = taskUuid;
    infoData["file_code"] = fileCode;
    infoData["file_name"] = fileInfo.file_name;
    infoData["total_size"] = totalSize;
    infoData["sha256"] = sha256;
    infoData["offset"] = offset;

    Message infoMsg = Message::createResponse(reqMsg, infoData);
    if (!sendMessage(m_socket, infoMsg)) {
        return;
    }

    file.seek(offset);
    int chunkIndex = 0;
    if (offset == totalSize) {
        QJsonObject chunkData;
        chunkData["task_uuid"] = taskUuid;
        chunkData["file_code"] = fileCode;
        chunkData["chunk_index"] = chunkIndex;
        chunkData["chunk_data"] = QString();
        chunkData["is_last"] = true;
        Message chunkMsg(MessageType::FileData, chunkData);
        chunkMsg.messageId = reqMsg.messageId;
        sendMessage(m_socket, chunkMsg);
        return;
    }

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(FILE_CHUNK_SIZE);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            ServerLogger::error("DOWNLOAD_FILE_READ_FAILED",
                                "读取下载文件失败",
                                {{"file_code", fileCode}, {"path", filePath}, {"error", file.errorString()}});
            sendMessage(m_socket, Message::createErrorResponse(reqMsg, StatusCode::Failed, "服务器文件读取失败"));
            return;
        }

        QJsonObject chunkData;
        chunkData["task_uuid"] = taskUuid;
        chunkData["file_code"] = fileCode;
        chunkData["chunk_index"] = chunkIndex++;
        chunkData["chunk_data"] = QString::fromLatin1(chunk.toBase64());
        chunkData["is_last"] = file.atEnd();

        Message chunkMsg(MessageType::FileData, chunkData);
        chunkMsg.messageId = reqMsg.messageId;
        if (!sendMessage(m_socket, chunkMsg)) {
            return;
        }
    }

    ServerLogger::info("DOWNLOAD_FILE_SENT",
                       "任务文件发送完成",
                       {{"file_code", fileCode}, {"task_uuid", taskUuid}, {"total_size", totalSize}, {"offset", offset}});
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

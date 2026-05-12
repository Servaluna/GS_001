#include "clienthandler.h"

#include "../dao/userdao.h"
#include "../services/userservice.h"

ClientHandler::ClientHandler(QTcpSocket* socket, QObject *parent)
    : QObject{parent}
    , m_socket(socket)
    , m_buffer()
    , m_expectedLength(0)
    , m_userDao(new UserDAO())
    , m_userService(new UserService(m_userDao))
    , m_lastActiveTime(0)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);

    emit logMessage("新客户端连接: " + socket->peerAddress().toString());
}

ClientHandler::~ClientHandler()
{
    delete m_userService;
    delete m_userDao;
    DEBUG_LOCATION << "ClientHandler 被删除:" << this;
}

void ClientHandler::onReadyRead()
{
    while (true) {
        Message msg = receiveMessage(m_socket, m_buffer, m_expectedLength);

        if (!msg.isValid()) {
            break;
        }

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
        DEBUG_LOCATION << "Session" << m_socket << "lastActiveTime:" << m_lastActiveTime;

        switch (msg.type) {
        case MessageType::LoginRequest:
            handleLoginRequest(msg);
            DEBUG_LOCATION << "消息类型:" << msg.type;
            break;

        default:
            DEBUG_LOCATION << "消息类型:" << msg.type;
            break;
        }
    }
}

void ClientHandler::onDisconnected()
{
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

    emit logMessage(QString("处理登录请求: %1").arg(username));

    const LoginResult loginResult = m_userService->login(username, password);

    if (loginResult.success) {
        emit logMessage(QString("登录成功: %1 (%2)").arg(username, loginResult.user.role));

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
        Message respMsg = Message::createResponse(reqMsg, loginResult.toResponseJson());
        sendMessage(m_socket, respMsg);
        emit loginSucceeded(loginResult.user.username, loginResult.user.role, loginResult.token);
        DEBUG_LOCATION << loginResult.toResponseJson();
        return;
    }

    emit logMessage(QString("登录失败: %1，原因: %2").arg(username, loginResult.errorMessage));
    Message errorMsg = Message::createErrorResponse(
        reqMsg,
        StatusCode::PermissionDenied,
        loginResult.errorMessage.isEmpty() ? "用户名或密码错误" : loginResult.errorMessage
    );
    sendMessage(m_socket, errorMsg);
}

#include "clienthandler.h"
#include "../database/models/user.h"
// #include "protocol.h"

ClientHandler::ClientHandler(QTcpSocket* socket , QObject *parent)
    : QObject{parent}
    ,m_socket(socket)
    ,m_buffer()
    ,m_expectedLength(0)
    ,m_lastActiveTime(0)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);

    emit logMessage("新客户端连接: " + socket->peerAddress().toString());
}

ClientHandler::~ClientHandler()
{
    DEBUG_LOCATION << "ClientHandler 被删除:" << this;
}

void ClientHandler::onReadyRead()
{
    // QByteArray data = m_socket->readAll();

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

    QString username = data["username"].toString();
    QString password = data["password"].toString();

    emit logMessage(QString("handleLoginRequest登录请求: %1").arg(username));

    // 验证用户（调用 User 模型）
    UserInfo userInfo = User::authenticate(username, password);

    QJsonObject responseData;

    if (userInfo.isValid()) {
        emit logMessage(QString("登录成功: %1 (%2)").arg(username,userInfo.role));

        // 生成简单 token（实际项目应使用 JWT）
        QString token = QString("%1_%2_%3")
                            .arg(userInfo.user_id)
                            .arg(userInfo.username)
                            .arg(QDateTime::currentMSecsSinceEpoch());

        // 对 token 进行简单哈希
        QByteArray tokenHash = QCryptographicHash::hash(
                                   token.toUtf8(),
                                   QCryptographicHash::Sha256
                                   ).toHex();

        // 存入待返回信息
        responseData["token"] = QString(tokenHash);
        responseData["user"] = QJsonObject{
            {"user_id", userInfo.user_id},
            {"username", userInfo.username},
            {"role", userInfo.role},
            {"status", userInfo.status}
        };

        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch(); // 保存最新登录时间

        Message respMsg = Message::createResponse(reqMsg, responseData);
        sendMessage(m_socket , respMsg);
        DEBUG_LOCATION<<responseData;

    } else {
        emit logMessage(QString("登录失败: %1").arg(username));
        Message errorMsg = Message::createErrorResponse(reqMsg,StatusCode::PermissionDenied,"用户名或密码错误");
        sendMessage(m_socket, errorMsg);
    }
}

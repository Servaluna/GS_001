#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

#define DEBUG_LOCATION qDebug().nospace() \
<< "[" << Q_FUNC_INFO \
       << " at " << __FILE__ << ":" << __LINE__ << "]"

// 消息类型枚举
namespace MessageType {
enum Type {
    Unknown = 0,

    // 登录相关 (1000-1999)
    LoginRequest = 1001,
    LoginResponse = 1002,
    Logout = 1003,
    RegisterRequest = 1004,
    RegisterResponse = 1005,

    // 用户管理 (2000-2999)
    GetUsers = 2001,
    UsersList = 2002,
    AddUser = 2003,
    UpdateUser = 2004,
    DeleteUser = 2005,

    // 升级包相关 (3000-3999)
    GetPackages = 3001,
    PackagesList = 3002,
    UploadPackage = 3003,
    DownloadPackage = 3004,

    // 设备相关 (4000-4999)
    GetDevices = 4001,
    DevicesList = 4002,
    ControlDevice = 4003,

    // 文件传输相关 (5000-5999)
    GetTaskFile = 5001,           // 请求获取任务文件
    TaskFileInfo = 5002,          // 文件信息响应
    DownloadFile = 5003,          // 下载文件
    FileData = 5004,              // 文件数据块
    FileTransferComplete = 5005,  // 文件传输完成
    FileVerifyRequest = 5006,      // 文件验证请求
    FileVerifyResponse = 5007,     // 文件验证响应

    // 设备安装相关 (6000-6999)
    InstallFile = 6001,            // 安装文件
    InstallProgress = 6002,        // 安装进度
    InstallResult = 6003,          // 安装结果
    DeviceReboot = 6004,            // 设备重启

    // 任务同步相关 (7000-7999)
    GetCurrentUserTasks = 7001,
    CurrentUserTasksList = 7002,
    UpdateTaskStatus = 7003,
    TaskStatusUpdated = 7004,

    // 错误码 (9000-9999)
    Error = 9001,
    InvalidRequest = 9002,
    Unauthorized = 9003
    };
}

// 消息状态码
namespace StatusCode {
enum Code {
    Success = 0,
    Failed = 1,
    InvalidParams = 2,
    NotFound = 3,
    AlreadyExists = 4,
    PermissionDenied = 5
    };
}

// 消息结构
class Message {
public:
    MessageType::Type type = MessageType::Unknown;  // 直接初始化，默认无效
    QString messageId;  // 消息ID，用于请求-响应配对
    qint64 timestamp;   // 时间戳
    QJsonObject data;

    Message() {
        messageId = QUuid::createUuid().toString();
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    explicit Message(MessageType::Type t, const QJsonObject& d = QJsonObject())
        : type(t), data(d) {
        messageId = QUuid::createUuid().toString();
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }

    // 序列化为字节流
    // compact 表示是否输出紧凑格式
    QByteArray toByteArray(bool compact = false) const {
        QJsonObject obj;
        obj["type"] = static_cast<int>(type);
        obj["messageId"] = messageId;
        obj["timestamp"] = timestamp;
        obj["data"] = data;

        QJsonDocument doc(obj);
        return compact ? doc.toJson(QJsonDocument::Compact) : doc.toJson();
    }

    // 从字节流解析
    static Message fromByteArray(const QByteArray& bytes) {
        Message msg;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);

        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON解析错误:" << error.errorString();
            return msg;  // 返回默认消息
        }

        if (!doc.isObject()) {
            qWarning() << "Invalid JSON format: not an object";
            return msg;
        }

        QJsonObject obj = doc.object();

        msg.type = static_cast<MessageType::Type>(obj["type"].toInt());
        msg.messageId = obj["messageId"].toString();
        msg.timestamp = obj["timestamp"].toInteger();
        msg.data = obj["data"].toObject();

        return msg;
    }

    // 判断消息是否有效
    bool isValid() const {
        return type != MessageType::Unknown;
    }

    // 创建成功响应
    static Message createResponse(const Message& request, const QJsonObject& responseData = QJsonObject()) {
        Message response;
        response.type = static_cast<MessageType::Type>(request.type + 1);  // 响应类型 = 请求类型+1（enum的时候注意）
        response.messageId = request.messageId;  // 使用相同的消息ID配对
        response.data["status"] = StatusCode::Success;
        response.data = responseData;
        return response;
    }

    // 创建错误响应
    static Message createErrorResponse(const Message& request,
                                       StatusCode::Code code,
                                       const QString& errorMsg) {
        Message response;
        response.type = MessageType::Error;
        response.messageId = request.messageId;
        response.data["reqMsgType"] = static_cast<int>(request.type);
        response.data["status"] = code;
        response.data["error"] = errorMsg;
        return response;
    }
};

// 内联函数：发送消息
inline bool sendMessage(QTcpSocket* socket, const Message& msg) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Socket is not connected";
        return false;
    }

    QByteArray data = msg.toByteArray(true);  // compact 紧凑格式

    // 添加长度前缀，解决 TCP 粘包问题
    QByteArray packet;
    quint32 len = data.size();
    packet.append(reinterpret_cast<const char*>(&len), 4);//按照字节进行访问
    packet.append(data);

    qint64 written = socket->write(packet);
    if (written != packet.size()) {
        qWarning() << "发送数据不完整:" << written << "/" << packet.size();
        return false;
    }

    socket->flush();
    DEBUG_LOCATION << "发送消息，类型:" << msg.type << "大小:" << data.size();
    return true;
}

// 内联函数：接收消息，根据长度前缀解决 TCP 粘包
inline Message receiveMessage(QTcpSocket* socket, QByteArray& buffer, quint32& expectedLength) {
    buffer.append(socket->readAll());

    // while 循环确保完整处理一条消息
    while (buffer.size() >= 4) {
        if (expectedLength == 0) {
            // 从缓冲区前 4 字节读取长度；用 memcpy 避免内存对齐问题
            memcpy(&expectedLength, buffer.constData(), 4);
            buffer.remove(0, 4);
        }

        if (buffer.size() >= expectedLength) {
            QByteArray msgData = buffer.left(expectedLength);
            buffer.remove(0, expectedLength);

            Message msg = Message::fromByteArray(msgData);
            expectedLength = 0;  // 重置

            if (msg.isValid()) {
                DEBUG_LOCATION << "收到消息，类型:" << msg.type;
                return msg;
            } else {
                qWarning() << "收到无效消息";
            }
        } else {
            break;  // 数据不够，等待更多数据
        }
    }

    return Message();  // 返回无效消息
}

#endif // PROTOCOL_H

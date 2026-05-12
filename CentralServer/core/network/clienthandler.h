#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include "protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QFileInfo>

class UserDAO;
class UserService;

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(QTcpSocket* socket, QObject *parent = nullptr);
    ~ClientHandler();

signals:
    void finished();
    void logMessage(const QString& msg);
    void loginSucceeded(QString username, QString role, QString token);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    quint32 m_expectedLength = 0;
    UserDAO* m_userDao = nullptr;
    UserService* m_userService = nullptr;

    quint64 m_lastActiveTime = 0; // 保存毫秒级时间戳

    void handleLoginRequest(const Message& reqMsg);
};

#endif // CLIENTHANDLER_H

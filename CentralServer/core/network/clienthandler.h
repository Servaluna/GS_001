#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include "protocol.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>

class UserDAO;
class UserService;
class TaskDAO;
class TaskService;

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
    void loggedOut(ClientHandler* handler);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void handleLoginRequest(const Message& reqMsg);
    void handleLogoutRequest(const Message& reqMsg);
    void handleCurrentUserTasksRequest(const Message& reqMsg);
    void handleTaskStatusUpdate(const Message& reqMsg);
    void handleTaskFileRequest(const Message& reqMsg);

    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    quint32 m_expectedLength = 0;
    UserDAO* m_userDao = nullptr;
    UserService* m_userService = nullptr;
    TaskDAO* m_taskDao = nullptr;
    TaskService* m_taskService = nullptr;
    int m_loggedInUserId = -1;
    int m_loggedInRoleId = 0;
    QString m_sessionToken;
    quint64 m_lastActiveTime = 0;
};

#endif // CLIENTHANDLER_H

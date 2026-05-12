#ifndef SERVER_H
#define SERVER_H

#include "ClientHandler.h"

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>

struct ClientInfo {
    QString address;           // IP地址
    quint16 port;              // 端口
    QDateTime connectTime;      // 连接时间
    ClientHandler* handler;     //处理器

    bool isLoggedIn;        // 是否已登录
    bool isDisconnected;    // 是否已断开
    QString username;          // 登录用户名
    QString role;              // 用户角色
    QString token;             // 认证token
    QDateTime loginTime;        // 登录时间

    QList<int> tasks;           // 该客户端的任务列表
    QMap<QString, qint64> fileTransfers; // 正在传输的文件

    ClientInfo() {
        address = "0.0.0.0";
        port = 0;
        connectTime = QDateTime::currentDateTime();
        isLoggedIn = false;
        isDisconnected = false;
    }

    ClientInfo(QTcpSocket* socket) {
        address = socket->peerAddress().toString();
        port = socket->peerPort();
        connectTime = QDateTime::currentDateTime();
        handler = nullptr;
        isLoggedIn = false;
        isDisconnected = false;
    }
};

QT_BEGIN_NAMESPACE
namespace Ui {
class Server;
}
QT_END_NAMESPACE

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QWidget *parent = nullptr);
    ~Server();
    bool start();
    void stop();

signals:
    void logMessage(const QString& msg);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    bool onStartServer();              // 启动服务器
    void onStopServer();               // 停止服务器

    // 处理 ClientHandler 的信号
    void onClientLog(const QString& msg);
    void onClientFinished(ClientHandler* handler);
    void onClientLoginSucceeded(QString username, QString role, QString token);

private:
    void updateClientList();           // 更新客户端列表显示
    void addClientToList(QTcpSocket *socket);   // 添加客户端到列表
    void removeClientFromList(QTcpSocket *socket); // 从列表移除客户端

    Ui::Server *ui;
    QTcpServer *m_server;
    bool m_isListening;                 // 服务器监听状态
    QMap<QTcpSocket*, ClientInfo> m_clients;;      // 用 QMap 保存所有 socket 和对应的客户端信息

};
#endif // SERVER_H

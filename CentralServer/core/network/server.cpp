#include "server.h"
#include "ui_server.h"

#define DEFAULT_PORT 8000

Server::Server(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Server)
    , m_server(nullptr)
    , m_isListening(false)
{
    ui->setupUi(this);
    ui->labelStatus->setText("服务器停止运行");
    ui->labelConnectionCount->setText("当前连接数: 0");

    this->setWindowTitle("服务端");

    m_server = new QTcpServer(this);

    connect(m_server , &QTcpServer::newConnection , this , &Server::onNewConnection);
    connect(ui->btnStart, &QPushButton::clicked, this, &Server::onStartServer);
    connect(ui->btnStop, &QPushButton::clicked, this, &Server::onStopServer);
}

Server::~Server()
{
    onStopServer();
    delete ui;
}

bool Server::start(){
    if(onStartServer()){
        return true;
    }else{
        return false;
    }
}

void Server::stop(){
    onStopServer();
}


void Server::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();

        DEBUG_LOCATION << "正在建立连接";

        // 连接信号
        // connect(socket, &QTcpSocket::readyRead, this, &Server::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);

        // 为每个客户端创建专门的处理器
        ClientHandler* handler = new ClientHandler(socket, this);

        //构造存储结构
        ClientInfo info(socket);
        info.handler = handler;
        m_clients[socket] = info;

        DEBUG_LOCATION << "新存储的1对1信息"
                 << m_clients[socket].address
                 << m_clients[socket].connectTime
                 << m_clients[socket].handler;

        // 连接处理器的信号
        connect(handler, &ClientHandler::logMessage, this, &Server::onClientLog);
        connect(handler, &ClientHandler::finished, [this, handler]() { onClientFinished(handler); });

        // 打印连接信息
        DEBUG_LOCATION << "新客户端连接:"
                 << "client地址:" << socket->peerAddress().toString()
                 << "client端口:" << socket->peerPort()
                 << "本端server端口:" << socket->localPort()
                 << "当前连接数:" << m_clients.size();

    }
    updateClientList();
}

void Server::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    //删除信息
    if (m_clients.contains(socket)) {
        ClientInfo info = m_clients[socket];
        m_clients.remove(socket);

    DEBUG_LOCATION << "客户端断开:"
             << "地址:" << info.address
             << "端口:" << info.port
             << "在线时长:" << info.connectTime.secsTo(QDateTime::currentDateTime()) << "秒";
    }

    socket->deleteLater();
    updateClientList();
}

bool Server::onStartServer()
{
    if (m_isListening) return 0;

    // 获取端口号
    bool ok;
    int port = ui->lineEditPort->text().toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        port = DEFAULT_PORT;
        ui->lineEditPort->setText(QString::number(port));
    }

    // 启动服务器监听
    if (m_server->listen(QHostAddress::Any, port)) {
        m_isListening = true;
        // ui->lineEditIP->setText(m_server->serverAddress().toString());
        ui->labelStatus->setText(QString("服务器已启动，监听端口: %1").arg(port));
        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(true);
        ui->lineEditPort->setEnabled(false);
        DEBUG_LOCATION << "服务器启动成功，监听端口:" << port;
        return 1;

    } else {
        ui->labelStatus->setText("服务器启动失败: " + m_server->errorString());
        DEBUG_LOCATION << "服务器启动失败:" << m_server->errorString();
        return 0;
    }
}

void Server::onStopServer()
{
    if (!m_isListening) return;

    // 断开所有客户端连接
    for (QTcpSocket* socket : m_clients.keys()) {  // 遍历所有socket
        ClientInfo info = m_clients[socket];

        DEBUG_LOCATION << "断开客户端:" << info.address << ":" << info.port;

        socket->disconnectFromHost();
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->waitForDisconnected(1000);
        }
        socket->deleteLater();
    }

    m_clients.clear();

    // 关闭服务器
    m_server->close();
    m_isListening = false;

    // 更新UI
    ui->labelStatus->setText("服务器停止运行");
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    updateClientList();  // 清空客户端列表

    DEBUG_LOCATION << "服务器已停止";
}

void Server::onClientLog(const QString& msg)
{
    DEBUG_LOCATION << "[ClientHandler]" << msg;
}

// 处理 ClientHandler 完成（客户端断开）的信号
void Server::onClientFinished(ClientHandler* handler)
{
    // 找到 handler 对应的 socket
    QTcpSocket* socket = nullptr;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value().handler == handler) {  // 如果 ClientInfo 中保存了 handler
            socket = it.key();
            break;
        }
    }

    if (socket) {
        // 从列表中移除
        ClientInfo info = m_clients[socket];
        m_clients.remove(socket);

        DEBUG_LOCATION << "ClientHandler finished for:" << info.address << ":" << info.port;

        // 更新 UI
        updateClientList();
    }
}

void Server::updateClientList()
{
    // 清空并重新填充客户端列表
    ui->listWidgetClients->clear();

    DEBUG_LOCATION << "更新客户端列表，当前连接数:" << m_clients.size();

    // 遍历QMap中的所有客户端
    int index = 1;
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        // QTcpSocket* socket = it.key();      // 获取socket
        ClientInfo& info = it.value();       // 获取客户端信息

        DEBUG_LOCATION << info.address;

        // 构建显示信息
        QString clientInfo;

        if (info.isLoggedIn) {
            // 已登录用户显示详细信息// clazy:excludeall=qstring-arg
            clientInfo = QString("%1.  %2:%3 [%4] %5 (%6) - 连接时间:%7")
                             .arg(index++)
                             .arg(info.address)
                             .arg(info.port)
                             .arg(info.isLoggedIn ? "已登录" : "未登录")
                             .arg(info.username)
                             .arg(info.role)
                             .arg(info.connectTime.toString("hh:mm:ss"));
        } else {
            // 未登录用户显示基本信息
            clientInfo = QString("%1. %2:%3 [未登录] - 连接时间:%4")
                             .arg(index++)
                             .arg(info.address)
                             .arg(info.port)
                             .arg(info.connectTime.toString("hh:mm:ss"));
        }

        ui->listWidgetClients->addItem(clientInfo);
    }

    // 更新状态栏显示连接数
    ui->labelConnectionCount->setText(QString("当前连接数: %1").arg(m_clients.size()));
}

void Server::addClientToList(QTcpSocket *socket)
{
    ClientInfo info(socket);
    m_clients[socket] = info;
    updateClientList();

    DEBUG_LOCATION << "客户端添加到列表:" << info.address << ":" << info.port;
}

void Server::removeClientFromList(QTcpSocket *socket)
{
    if (m_clients.contains(socket)) {
        ClientInfo info = m_clients[socket];
        DEBUG_LOCATION << "客户端从列表移除:" << info.address << ":" << info.port;
        m_clients.remove(socket);
    }
    updateClientList();
}


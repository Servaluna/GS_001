#include "serverwindow.h"
#include "ui_serverwindow.h"

#include "../core/network/server.h"

#include <QDebug>

constexpr quint16 DEFAULT_PORT = 8000;

ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ServerWindow)
    , m_server(new Server(this))
{
    ui->setupUi(this);
    setWindowTitle("服务端");

    ui->lineEditPort->setText(QString::number(DEFAULT_PORT));
    ui->labelStatus->setText("服务器停止运行");
    ui->labelConnectionCount->setText("当前连接数: 0");
    setListeningUi(false);

    connect(ui->btnStart, &QPushButton::clicked, this, &ServerWindow::on_btnStart_clicked);
    connect(ui->btnStop, &QPushButton::clicked, this, &ServerWindow::on_btnStop_clicked);

    connect(m_server, &Server::started, this, &ServerWindow::onServerStarted);
    connect(m_server, &Server::stopped, this, &ServerWindow::onServerStopped);
    connect(m_server, &Server::startFailed, this, &ServerWindow::onServerStartFailed);
    connect(m_server, &Server::clientListChanged, this, &ServerWindow::updateClientList);
    connect(m_server, &Server::logMessage, this, [](const QString& msg) {
        qDebug() << "[Server]" << msg;
    });
}

ServerWindow::~ServerWindow()
{
    m_server->stop();
    delete ui;
}

bool ServerWindow::startServer()
{
    return m_server->start(configuredPort());
}

void ServerWindow::on_btnStart_clicked()
{
    startServer();
}

void ServerWindow::on_btnStop_clicked()
{
    m_server->stop();
}

void ServerWindow::onServerStarted(quint16 port)
{
    ui->labelStatus->setText(QString("服务器已启动，监听端口: %1").arg(port));
    setListeningUi(true);
    updateClientList();
}

void ServerWindow::onServerStopped()
{
    ui->labelStatus->setText("服务器停止运行");
    setListeningUi(false);
    updateClientList();
}

void ServerWindow::onServerStartFailed(const QString& error)
{
    ui->labelStatus->setText(QString("服务器启动失败: %1").arg(error));
    setListeningUi(false);
}

void ServerWindow::updateClientList()
{
    ui->listWidgetClients->clear();

    int index = 1;
    const QList<ClientInfo> clients = m_server->clients();
    for (const ClientInfo& info : clients) {
        ui->listWidgetClients->addItem(clientDisplayText(info, index++));
    }

    ui->labelConnectionCount->setText(QString("当前连接数: %1").arg(m_server->activeClientCount()));
}

quint16 ServerWindow::configuredPort() const
{
    bool ok = false;
    int port = ui->lineEditPort->text().toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        port = DEFAULT_PORT;
        ui->lineEditPort->setText(QString::number(port));
    }
    return static_cast<quint16>(port);
}

QString ServerWindow::clientDisplayText(const ClientInfo& info, int index) const
{
    const QString loginState = info.isDisconnected
        ? "断开"
        : (info.isLoggedIn ? "已登录" : "未登录");

    if (info.isLoggedIn && !info.isDisconnected) {
        return QString("%1. %2:%3 [%4] %5 (%6) - 连接时间:%7")
            .arg(index, 2)
            .arg(info.address, 15)
            .arg(info.port, 5)
            .arg(loginState, 6)
            .arg(info.username, 10)
            .arg(info.role, 8)
            .arg(info.connectTime.toString("hh:mm:ss"), 8);
    }

    return QString("%1. %2:%3 [%4] - 连接时间:%5")
        .arg(index, 2)
        .arg(info.address, 15)
        .arg(info.port, 5)
        .arg(loginState, 6)
        .arg(info.connectTime.toString("hh:mm:ss"), 8);
}

void ServerWindow::setListeningUi(bool listening)
{
    ui->btnStart->setEnabled(!listening);
    ui->btnStop->setEnabled(listening);
    ui->lineEditPort->setEnabled(!listening);
}

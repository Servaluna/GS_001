#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setConnectionStatus(bool connected, const QString& message)
{
    m_connected = connected;

    ui->labelAdgStatusValue->setText(connected ? "已连接GS客户端" : "未连接GS客户端");
    ui->btnToggleGsConnection->setText(connected ? "断开 GS" : "连接 GS");
    statusBar()->showMessage(message);

    setWindowTitle(connected
        ? "AC-1002 飞机模拟器 - 已连接"
        : "AC-1002 飞机模拟器 - 未连接");
}

void MainWindow::on_btnToggleGsConnection_clicked()
{
    emit toggleGroundStationConnectionRequested();
}

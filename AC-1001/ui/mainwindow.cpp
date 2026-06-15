#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("AC-1001 飞机模拟器");

    connect(ui->btnToggleGsConnection, &QPushButton::clicked,
            this, &MainWindow::toggleGroundStationConnectionRequested);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setConnectionStatus(bool connected, const QString& message)
{
    m_connected = connected;

    ui->labelAdgStatusValue->setText(connected ? "已连接" : "未连接");
    ui->labelCableStatusValue->setText(connected ? "网线已接入" : "网线未接入");
    ui->btnToggleGsConnection->setText(connected ? "断开 GS" : "连接 GS");
    statusBar()->showMessage(message);

    setWindowTitle(connected
        ? "AC-1001 飞机模拟器 - 已连接"
        : "AC-1001 飞机模拟器 - 未连接");
}

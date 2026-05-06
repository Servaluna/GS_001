#include "appcontroller.h"

#include "../core/network/deviceconnector.h"
#include "../core/network/serverconnector.h"
#include "../ui/deviceconnectorwindow.h"
#include "../ui/logindialog.h"
#include "../ui/mainwindow.h"

#include <QApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <QTimer>

AppController::AppController(QObject *parent)
    : QObject{parent}
{}

int AppController::start()
{
    if (!connectToCentralServer()) {
        return -1;
    }

    connect(&ServerConnector::instance(), &ServerConnector::loginSuccess,
            this, &AppController::onLoginSuccess);

    showLoginPage();
    return 0;
}

bool AppController::connectToCentralServer()
{
    ServerConnector& server = ServerConnector::instance();
    server.connectToServer("127.0.0.1", 8000);

    QEventLoop connectLoop;
    QTimer connectTimeout;
    connectTimeout.setSingleShot(true);

    connect(&server, &ServerConnector::connected, &connectLoop, &QEventLoop::quit);
    connect(&server, &ServerConnector::errorOccurred, &connectLoop, &QEventLoop::quit);
    connect(&connectTimeout, &QTimer::timeout, &connectLoop, &QEventLoop::quit);

    connectTimeout.start(3000);
    connectLoop.exec();

    if (!server.isConnected()) {
        qCritical() << "无法连接到服务器 CentralServer 127.0.0.1:8000";
        qCritical() << "请确保服务器已启动";
        QMessageBox::critical(nullptr,
                              "连接失败",
                              "无法连接到服务器 CentralServer 127.0.0.1:8000，请确保服务器已启动。");
        return false;
    }

    qDebug() << "服务器 CentralServer 连接成功";
    return true;
}

void AppController::showLoginPage()
{
    closeDeviceConnectorPage();
    closeMainPage();

    if (!m_loginDialog) {
        m_loginDialog = new LoginDialog();
        connect(m_loginDialog, &QObject::destroyed, this, [this]() {
            m_loginDialog.clear();
        });
        connect(m_loginDialog, &QDialog::rejected, qApp, &QApplication::quit);
    }

    m_loginDialog->show();
    m_loginDialog->raise();
    m_loginDialog->activateWindow();
}

void AppController::showMainPage(const QString& token, const UserInfo& userInfo)
{
    closeMainPage();

    m_mainWindow = new MainWindow(token, userInfo);
    connect(m_mainWindow, &QObject::destroyed, this, [this]() {
        m_mainWindow.clear();
    });
    connect(m_mainWindow, &MainWindow::openDeviceConnector,
            this, &AppController::onOpenDeviceConnectorRequested);
    connect(m_mainWindow, &MainWindow::logoutFromMainWindow,
            this, &AppController::onLogoutRequested);

    m_mainWindow->show();
    closeLoginPage();
}

void AppController::onLoginSuccess(QString token, const UserInfo& userInfo)
{
    showMainPage(token, userInfo);
}

void AppController::onLogoutRequested()
{
    closeDeviceConnectorPage();
    closeMainPage();
    showLoginPage();
}

void AppController::onOpenDeviceConnectorRequested()
{
    if (!m_mainWindow) {
        return;
    }

    if (!m_deviceConnector) {
        m_deviceConnector = new DeviceConnector(this);
    }

    if (!m_deviceConnectorWindow) {
        m_deviceConnectorWindow = new deviceconnectorwindow(m_deviceConnector, m_mainWindow);
        connect(m_deviceConnectorWindow, &QObject::destroyed, this, [this]() {
            m_deviceConnectorWindow.clear();
        });
    }

    m_deviceConnectorWindow->show();
    m_deviceConnectorWindow->raise();
    m_deviceConnectorWindow->activateWindow();
}

void AppController::closeLoginPage()
{
    if (m_loginDialog) {
        m_loginDialog->hide();
        m_loginDialog->deleteLater();
        m_loginDialog.clear();
    }
}

void AppController::closeMainPage()
{
    if (m_mainWindow) {
        m_mainWindow->hide();
        m_mainWindow->deleteLater();
        m_mainWindow.clear();
    }
}

void AppController::closeDeviceConnectorPage()
{
    if (m_deviceConnectorWindow) {
        m_deviceConnectorWindow->hide();
        m_deviceConnectorWindow->deleteLater();
        m_deviceConnectorWindow.clear();
    }
}

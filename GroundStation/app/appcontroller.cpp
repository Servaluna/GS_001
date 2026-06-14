#include "appcontroller.h"

#include "../core/services/taskservice.h"
#include "../core/logging/logger.h"
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
    Logger::info("APP_INIT", "初始化地面站应用控制器");

    if (!connectToCentralServer()) {
        return -1;
    }

    m_deviceConnector = new DeviceConnector(this);
    m_taskService = new TaskService(this);
    if (!m_taskService->init(&ServerConnector::instance(), m_deviceConnector)) {
        Logger::error("APP_INIT_FAILED", "任务服务初始化失败");
        return -1;
    }
    m_taskService->start();

    connect(&ServerConnector::instance(), &ServerConnector::loginSuccess,
            this, &AppController::onLoginSuccess);

    showLoginPage();
    return 0;
}

bool AppController::connectToCentralServer()
{
    ServerConnector& server = ServerConnector::instance();
    LogContext serverContext;
    serverContext.ip_address = "127.0.0.1";
    Logger::info("SERVER_CONNECT_START", "开始连接 CentralServer",
                 serverContext,
                 {{"host", "127.0.0.1"}, {"port", 8000}});
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
        Logger::error("SERVER_CONNECT_FAILED",
                      "无法连接到服务器 CentralServer 127.0.0.1:8000",
                      serverContext,
                      {{"host", "127.0.0.1"}, {"port", 8000}, {"timeout_ms", 3000}});
        QMessageBox::critical(nullptr,
                              "连接失败",
                              "无法连接到服务器 CentralServer 127.0.0.1:8000，请确认服务器已经启动，并且监听地址和端口正确。");
        return false;
    }

    Logger::info("SERVER_CONNECTED", "服务器 CentralServer 连接成功",
                 serverContext,
                 {{"host", "127.0.0.1"}, {"port", 8000}});
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

    m_mainWindow = new MainWindow(token, userInfo, m_taskService);
    connect(m_mainWindow, &QObject::destroyed, this, [this]() {
        m_mainWindow.clear();
    });
    connect(m_mainWindow, &MainWindow::openDeviceConnector,
            this, &AppController::onOpenDeviceConnectorRequested);
    connect(m_mainWindow, &MainWindow::logoutFromMainWindow,
            this, &AppController::onLogoutRequested);
    connect(m_taskService, &TaskService::taskFinished,
            m_mainWindow, [this]() {
                if (m_mainWindow) {
                    m_mainWindow->showExecutePageAndReload();
                }
            });

    m_mainWindow->show();
    closeLoginPage();
}

void AppController::onLoginSuccess(QString token, const UserInfo& userInfo)
{
    Logger::instance().setOperatorUserId(userInfo.user_id);
    Logger::instance().setSessionId(token);
    LogContext loginContext;
    loginContext.operator_user_id = userInfo.user_id;
    loginContext.session_id = token;
    Logger::info("AUTH_LOGIN_SUCCESS",
                 QString("用户 %1 登录成功").arg(userInfo.username),
                 loginContext,
                 {{"username", userInfo.username}, {"role", userInfo.role}, {"role_id", userInfo.role_id}});
    showMainPage(token, userInfo);
}

void AppController::onLogoutRequested()
{
    Logger::info("AUTH_LOGOUT", "用户退出登录");
    ServerConnector::instance().logoutRequest();
    Logger::instance().setOperatorUserId(-1);
    Logger::instance().setSessionId(QString());
    closeDeviceConnectorPage();
    closeMainPage();
    showLoginPage();
}

void AppController::onOpenDeviceConnectorRequested()
{
    if (!m_mainWindow || !m_deviceConnector) {
        Logger::warn("CMC_CONNECT_WINDOW_FAILED", "无法打开设备连接窗口，主窗口或设备连接器未初始化");
        return;
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
    Logger::info("CMC_CONNECT_WINDOW_OPENED", "打开设备连接窗口");
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

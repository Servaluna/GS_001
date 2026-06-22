#include "appcontroller.h"

#include "../core/logging/logger.h"
#include "../core/localdatabase/localdatabase.h"
#include "../core/network/deviceconnector.h"
#include "../core/network/serverconnector.h"
#include "../core/services/taskservice.h"
#include "../ui/logindialog.h"
#include "../ui/mainwindow.h"

#include <QApplication>
#include <QEventLoop>
#include <QMessageBox>
#include <QTimer>

namespace {
constexpr quint16 AIRCRAFT_LISTEN_PORT = 9001;
}

AppController::AppController(QObject *parent)
    : QObject{parent}
{}

int AppController::start()
{
    Logger::info("APP_INIT", "初始化地面站应用控制器");

    if (!connectToCentralServer()) {
        return -1;
    }

    if (!LocalDatabase::getInstance()->init()) {
        Logger::error("APP_INIT_FAILED", "本地数据库初始化失败");
        QMessageBox::critical(nullptr,
                              "本地数据库初始化失败",
                              "无法初始化本地任务数据库，程序将退出。");
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
    connect(&ServerConnector::instance(), &ServerConnector::disconnected,
            this, &AppController::onCentralServerDisconnected);

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
                      "无法连接服务器 CentralServer 127.0.0.1:8000",
                      serverContext,
                      {{"host", "127.0.0.1"}, {"port", 8000}, {"timeout_ms", 3000}});
        QMessageBox::critical(nullptr,
                              "无法连接服务器",
                              "无法连接服务器，程序将退出。");
        return false;
    }

    Logger::info("SERVER_CONNECTED", "服务器 CentralServer 连接成功",
                 serverContext,
                 {{"host", "127.0.0.1"}, {"port", 8000}});
    return true;
}

void AppController::showLoginPage()
{
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
    connect(m_mainWindow, &MainWindow::logoutFromMainWindow,
            this, &AppController::onLogoutRequested);
    connect(m_taskService, &TaskService::taskFinished,
            m_mainWindow, [this]() {
                if (m_mainWindow) {
                    m_mainWindow->showExecutePageAndReload();
                }
            });
    connect(m_taskService, &TaskService::currentUserTasksSynced,
            m_mainWindow, [this](bool success, int aircraftTaskCount, int deviceTaskCount) {
                Logger::info("TASK_SYNC_REFRESH_MAINWINDOW",
                             "任务同步完成，刷新执行页面任务表",
                             {{"success", success},
                              {"aircraft_task_count", aircraftTaskCount},
                              {"device_task_count", deviceTaskCount}});
                if (m_mainWindow) {
                    m_mainWindow->showExecutePageAndReload();
                }
            });
    connect(m_deviceConnector, &DeviceConnector::cmcConnectionChanged,
            m_mainWindow, [this](bool connected, const QString&) {
                if (m_mainWindow) {
                    m_mainWindow->setAircraftConnectionStatus(connected);
                }
            });

    m_mainWindow->setNetworkConnectionStatus(ServerConnector::instance().isConnected());
    m_mainWindow->setAircraftConnectionStatus(m_deviceConnector && m_deviceConnector->isConnected());
    m_mainWindow->show();
    closeLoginPage();
}

void AppController::onLoginSuccess(QString token, const UserInfo& userInfo)
{
    if (!m_deviceConnector->startListening(AIRCRAFT_LISTEN_PORT)) {
        QMessageBox::critical(nullptr,
                              "飞机连接监听失败",
                              QString("无法监听飞机连接端口 %1，无法进入地面站主界面。").arg(AIRCRAFT_LISTEN_PORT));
        return;
    }

    Logger::instance().setOperatorUserId(userInfo.user_id);
    Logger::instance().setSessionId(token);
    LogContext loginContext;
    loginContext.operator_user_id = userInfo.user_id;
    loginContext.session_id = token;
    Logger::info("AUTH_LOGIN_SUCCESS",
                 QString("用户 %1 登录成功").arg(userInfo.username),
                 loginContext,
                 {{"username", userInfo.username}, {"role", userInfo.role}, {"role_id", userInfo.role_id}});
    m_pendingToken = token;
    m_pendingUserInfo = userInfo;
    QTimer::singleShot(0, this, &AppController::syncTasksAndShowMainPage);
}

void AppController::syncTasksAndShowMainPage()
{
    if (!m_taskService || !m_pendingUserInfo.isValid()) {
        return;
    }

    LogContext loginContext;
    loginContext.operator_user_id = m_pendingUserInfo.user_id;
    loginContext.session_id = m_pendingToken;
    if (!m_taskService->syncTasksForUser(m_pendingUserInfo.user_id, m_pendingUserInfo.role_id)) {
        Logger::warn("TASK_SYNC_FAILED_AFTER_LOGIN",
                     "登录后同步任务失败，将使用本地缓存",
                     loginContext,
                     {{"username", m_pendingUserInfo.username}, {"role_id", m_pendingUserInfo.role_id}});
    }
    showMainPage(m_pendingToken, m_pendingUserInfo);
    m_pendingToken.clear();
    m_pendingUserInfo = UserInfo();
}

void AppController::onLogoutRequested()
{
    Logger::info("AUTH_LOGOUT", "用户退出登录");
    if (m_deviceConnector) {
        m_deviceConnector->stopListening();
    }
    ServerConnector::instance().logoutRequest();
    if (m_taskService && !m_taskService->clearLocalTaskData()) {
        Logger::warn("TASK_LOCAL_CLEAR_FAILED_AFTER_LOGOUT",
                     "退出登录后清空本地任务数据失败");
    }
    Logger::instance().setOperatorUserId(-1);
    Logger::instance().setSessionId(QString());
    closeMainPage();
    showLoginPage();
}

void AppController::onCentralServerDisconnected()
{
    Logger::warn("SERVER_DISCONNECTED_DURING_SESSION",
                 "与 CentralServer 的连接已断开，地面站客户端退出");

    if (m_deviceConnector) {
        m_deviceConnector->stopListening();
    }
    QMessageBox::critical(nullptr,
                          "无法连接服务器",
                          "无法连接服务器，程序将退出。");
    closeMainPage();
    closeLoginPage();
    qApp->quit();
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

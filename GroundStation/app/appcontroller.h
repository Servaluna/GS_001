#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include "models.h"

#include <QObject>
#include <QPointer>
#include <QString>

class DeviceConnector;
class LoginDialog;
class MainWindow;
class TaskService;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);

    int start();

private slots:
    void onLoginSuccess(QString token, const UserInfo& userInfo);
    void onLogoutRequested();
    void onOpenDeviceConnectorRequested();
    void onCentralServerDisconnected();
    void syncTasksAndShowMainPage();

private:
    bool connectToCentralServer();
    void showLoginPage();
    void showMainPage(const QString& token, const UserInfo& userInfo);
    void closeLoginPage();
    void closeMainPage();
    void closeDeviceConnectorPage();

    QPointer<LoginDialog> m_loginDialog;
    QPointer<MainWindow> m_mainWindow;
    QPointer<DeviceConnector> m_deviceConnector;
    QPointer<TaskService> m_taskService;
    QString m_pendingToken;
    UserInfo m_pendingUserInfo;
};

#endif // APPCONTROLLER_H

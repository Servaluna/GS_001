#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include "models.h"

#include <QObject>
#include <QPointer>
#include <QString>

class DeviceConnector;
class LoginDialog;
class MainWindow;
class deviceconnectorwindow;

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

private:
    bool connectToCentralServer();
    void showLoginPage();
    void showMainPage(const QString& token, const UserInfo& userInfo);
    void closeLoginPage();
    void closeMainPage();
    void closeDeviceConnectorPage();

    QPointer<LoginDialog> m_loginDialog;
    QPointer<MainWindow> m_mainWindow;
    QPointer<deviceconnectorwindow> m_deviceConnectorWindow;
    QPointer<DeviceConnector> m_deviceConnector;
};

#endif // APPCONTROLLER_H

#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>

class Server;
struct ClientInfo;

QT_BEGIN_NAMESPACE
namespace Ui {
class ServerWindow;
}
QT_END_NAMESPACE

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow();

    bool startServer();

private slots:
    void on_btnStart_clicked();
    void on_btnStop_clicked();
    void onServerStarted(quint16 port);
    void onServerStopped();
    void onServerStartFailed(const QString& error);
    void updateClientList();

private:
    quint16 configuredPort() const;
    QString clientDisplayText(const ClientInfo& info, int index) const;
    void setListeningUi(bool listening);

    Ui::ServerWindow *ui;
    Server* m_server;
};

#endif // SERVERWINDOW_H

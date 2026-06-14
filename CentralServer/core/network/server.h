#ifndef SERVER_H
#define SERVER_H

#include "clienthandler.h"

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

struct ClientInfo {
    QString address;
    quint16 port = 0;
    QDateTime connectTime;
    ClientHandler* handler = nullptr;

    bool isLoggedIn = false;
    bool isDisconnected = false;
    QString username;
    QString role;
    QString token;
    QDateTime loginTime;

    QList<int> tasks;
    QMap<QString, qint64> fileTransfers;

    ClientInfo()
        : address("0.0.0.0")
        , connectTime(QDateTime::currentDateTime())
    {}

    explicit ClientInfo(QTcpSocket* socket)
        : address(socket ? socket->peerAddress().toString() : QString("0.0.0.0"))
        , port(socket ? socket->peerPort() : 0)
        , connectTime(QDateTime::currentDateTime())
    {}
};

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject *parent = nullptr);
    ~Server();

    bool start(quint16 port = 8000);
    void stop();
    bool isListening() const;
    quint16 listenPort() const;
    int activeClientCount() const;
    QList<ClientInfo> clients() const;
    QString lastError() const;

signals:
    void started(quint16 port);
    void stopped();
    void startFailed(const QString& error);
    void clientListChanged();
    void logMessage(const QString& msg);

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onClientLog(const QString& msg);
    void onClientFinished(ClientHandler* handler);
    void onClientLoginSucceeded(QString username, QString role, QString token);
    void onClientLoggedOut(ClientHandler* handler);

private:
    void markSocketDisconnected(QTcpSocket* socket);
    QTcpSocket* socketForHandler(ClientHandler* handler) const;

    QTcpServer* m_server = nullptr;
    bool m_isListening = false;
    quint16 m_listenPort = 0;
    QString m_lastError;
    QMap<QTcpSocket*, ClientInfo> m_clients;
};

#endif // SERVER_H

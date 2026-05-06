#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include "protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QFileInfo>

class ClientHandler : public QObject
{
    Q_OBJECT
public:
    explicit ClientHandler(QTcpSocket* socket, QObject *parent = nullptr);
    ~ClientHandler();

signals:
    void finished();
    void logMessage(const QString& msg);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
    quint32 m_expectedLength = 0;

    quint64 m_lastActiveTime = 0;// 存储毫秒级时间戳,性能更高

    void handleLoginRequest(const Message& reqMsg);
};

#endif // CLIENTHANDLER_H

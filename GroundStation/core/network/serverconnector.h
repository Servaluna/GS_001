#ifndef SERVERCONNECTOR_H
#define SERVERCONNECTOR_H

#include "protocol.h"
#include "models.h"

#include <QDialog>
#include <QMessageBox>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonObject>
#include <QFileInfo>

class ServerConnector : public QObject
{
    Q_OBJECT

public:
    static ServerConnector& instance();

    bool connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const { return m_socket && m_socket->state() == QAbstractSocket::ConnectedState; }


    void loginRequest(const QString& username, const QString& password);
    // 请求下载文件（支持断点续传）
    bool fileDownloadRequest(const QString& fileCode, qint64 offset = 0, const QString& taskUuid = QString());

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& msg);


    void loginSuccess(QString token, const UserInfo& userInfo);
    // 文件信息接收
    void fileInfoReceived(QString taskId, qint64 totalSize, const QString& sha256);
    // 文件块接收
    void fileChunkReceived(QString taskId, const QByteArray& chunkData, int chunkIndex, bool isLast);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    explicit ServerConnector(QObject *parent = nullptr);
    ~ServerConnector();

    void cleanupSocket();
    void doConnect(const QString& host, quint16 port);

    void handleLoginResponse( const Message& respMsg);

    QTcpSocket* m_socket = nullptr;
    QString m_host;
    quint16 m_port;

    QString m_pendingHost;
    quint16 m_pendingPort;
    bool m_pendingConnection = false;

    QByteArray m_buffer;
    quint32 m_expectedLength = 0;
    quint64 m_lastActiveTime = 0;

};

#endif // SERVERCONNECTOR_H

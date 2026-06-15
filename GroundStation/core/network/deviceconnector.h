#ifndef DEVICECONNECTOR_H
#define DEVICECONNECTOR_H

#include <QFile>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

struct DeviceStatus {
    QString deviceId;
    QString deviceName;
    bool isOnline;
    QString version;
    QString lastUpdateTime;

    DeviceStatus() : isOnline(false) {}
};

class DeviceConnector : public QObject
{
    Q_OBJECT

public:
    explicit DeviceConnector(QObject *parent = nullptr);
    ~DeviceConnector();

    bool startListening(quint16 port = 9001);
    void stopListening();
    bool isConnected() const;

    void sendFileToDevice(const QString& taskId,
                          const QString& targetDeviceId,
                          const QString& localPath,
                          const QString& fileName,
                          const QString& sha256);

    DeviceStatus getDeviceStatus(const QString& deviceId) const;
    QList<DeviceStatus> getAllDeviceStatus() const;
    bool isDeviceOnline(const QString& deviceId) const;

signals:
    void cmcConnectionChanged(bool connected, QString errorMessage);
    void deviceStatusFullUpdated(QList<DeviceStatus> devices);
    void deviceStatusIncrementalUpdated(QList<DeviceStatus> devices);

    void sendProgress(QString taskId, qint64 sent, qint64 total, int percent);
    void sendFinished(QString taskId, bool success, QString message);
    void installResult(QString taskId, QString deviceId, bool success, QString message);

private slots:
    void onNewConnection();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onReadyRead();
    void onSendFileData();

private:
    enum class Command : quint8 {
        DeviceStatusFull = 0x01,
        DeviceStatusUpdate = 0x02,
        FileReceiveResult = 0x03,
        InstallResult = 0x04,
        FileStart = 0x10,
        FileData = 0x11,
        FileEnd = 0x12,
        Error = 0xFF
    };

    bool sendCommand(Command cmd, const QByteArray& data = QByteArray());
    bool sendFileStart(const QString& taskId,
                       const QString& targetDeviceId,
                       const QString& fileName,
                       qint64 fileSize,
                       const QString& sha256);
    bool sendFileEnd();

    QByteArray buildPacket(Command cmd, const QByteArray& payload);
    bool parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload);

    void handleDeviceStatusFull(const QByteArray& payload);
    void handleDeviceStatusUpdate(const QByteArray& payload);
    void handleFileReceiveResult(const QByteArray& payload);
    void handleInstallResult(const QByteArray& payload);
    void cleanupFileTransfer();
    void cleanupClientConnection();

    QTcpSocket* clientSocket() const;

    QTcpServer* m_server;
    QTcpSocket* m_socket;
    QString m_cmcIp;
    quint16 m_cmcPort;
    bool m_isConnected;
    QByteArray m_receiveBuffer;

    QMap<QString, DeviceStatus> m_deviceStatusMap;

    QString m_currentTaskId;
    QString m_currentTargetDeviceId;
    QFile* m_currentFile;
    qint64 m_fileSize;
    qint64 m_sentBytes;
    QString m_fileSha256;

    static constexpr int SEND_BUFFER_SIZE = 64 * 1024;
    static constexpr int SEND_INTERVAL_MS = 10;
    static constexpr quint16 PACKET_START_MARK = 0x5A5A;
    static constexpr int PACKET_HEADER_SIZE = 9;
    static constexpr quint16 DEFAULT_LISTEN_PORT = 9001;
};

#endif // DEVICECONNECTOR_H

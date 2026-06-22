#ifndef AIRCRAFTCLIENT_H
#define AIRCRAFTCLIENT_H

#include <QObject>
#include <QTcpSocket>

class AircraftSimulator;

class AircraftClient : public QObject
{
    Q_OBJECT

public:
    explicit AircraftClient(AircraftSimulator* simulator, QObject *parent = nullptr);

    void connectToGroundStation(const QString& host = QStringLiteral("127.0.0.1"), quint16 port = 9001);
    void disconnectFromGroundStation();
    bool isConnected() const;

signals:
    void connectionChanged(bool connected, QString message);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onReadyRead();

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

    struct TransferContext {
        QString taskId;
        QString targetDeviceId;
        QString fileName;
        QString tempPath;
        QString localPath;
        qint64 expectedSize = 0;
        qint64 receivedSize = 0;
        QString expectedSha256;
    };

    bool sendCommand(Command cmd, const QByteArray& payload = QByteArray());
    QByteArray buildPacket(Command cmd, const QByteArray& payload) const;
    bool parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload);

    void sendDeviceStatusFull();
    void sendFileReceiveResult(bool success, const QString& message);
    void sendInstallResult(bool success, const QString& message);
    void sendInstallResultForTask(const QString& taskId,
                                  const QString& targetDeviceId,
                                  bool success,
                                  const QString& message);
    QString receiveDirectory() const;
    QString buildLocalPackagePath(const QString& taskId, const QString& targetDeviceId, const QString& fileName) const;
    bool verifyPackageFile(const QString& filePath, qint64 expectedSize, const QString& expectedSha256) const;
    void handleFileStart(const QByteArray& payload);
    void handleFileData(const QByteArray& payload);
    void handleFileEnd();

    AircraftSimulator* m_simulator;
    QTcpSocket* m_socket;
    QString m_lastHost;
    quint16 m_lastPort;
    QByteArray m_receiveBuffer;
    TransferContext m_transfer;

    static constexpr quint16 PACKET_START_MARK = 0x5A5A;
    // Fixed packet overhead: 2-byte start mark + 1-byte command
    // + 4-byte payload size + 2-byte checksum.
    static constexpr int PACKET_HEADER_SIZE = 9;
};

#endif // AIRCRAFTCLIENT_H

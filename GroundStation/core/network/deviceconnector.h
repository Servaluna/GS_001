#ifndef DEVICECONNECTOR_H
#define DEVICECONNECTOR_H

#include <QObject>
#include <QTcpSocket>
#include <QMessageBox>
#include <QFile>

struct DeviceStatus {
    QString deviceId;      // 设备ID
    QString deviceName;    // 设备名称
    bool isOnline;         // 是否在线
    QString version;       // 当前版本
    QString lastUpdateTime;// 最后更新时间

    DeviceStatus() : isOnline(false) {}
};

class DeviceConnector : public QObject
{
    Q_OBJECT

public:
    explicit DeviceConnector(QObject *parent = nullptr);
    ~DeviceConnector();

    bool connectToCMC(const QString& ip, quint16 port);
    void disconnectFromCMC();
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
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onReadyRead();

    void onSendFileData();

    // void on_btnCancel_clicked();
    // void on_btnConnect_clicked();

private:
    // ========== 协议定义 ==========

    enum class Command : quint8 {
        // CMC → 地面站
        DeviceStatusFull = 0x01,   // 全量设备状态表
        DeviceStatusUpdate = 0x02, // 增量设备状态更新
        FileReceiveResult = 0x03,  // CMC 文件接收结果
        InstallResult = 0x04,      // 设备安装结果（CMC转发）

        // 地面站 → CMC
        FileStart = 0x10,          // 开始发送文件
        FileData = 0x11,           // 文件数据分片
        FileEnd = 0x12,            // 文件发送完成

        Error = 0xFF
    };

    // ========== 数据包处理 ==========

    bool sendCommand(Command cmd, const QByteArray& data = QByteArray());
    bool sendFileStart(const QString& taskId,
                       const QString& targetDeviceId,
                       const QString& fileName,
                       qint64 fileSize,
                       const QString& sha256);
    bool sendFileEnd();

    QByteArray buildPacket(Command cmd, const QByteArray& payload);
    bool parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload);

    // ========== 命令处理 ==========

    void handleDeviceStatusFull(const QByteArray& payload);
    void handleDeviceStatusUpdate(const QByteArray& payload);
    void handleFileReceiveResult(const QByteArray& payload);
    void handleInstallResult(const QByteArray& payload);

    // ========== 辅助方法 ==========

    void cleanupFileTransfer();
    // void updateDeviceStatus(const DeviceStatus& status, bool isFullUpdate);

private:
    QTcpSocket* m_socket;
    QString m_cmcIp;
    quint16 m_cmcPort;
    bool m_isConnected;
    QByteArray m_receiveBuffer;  // 粘包处理缓冲区

    // ========== 设备状态缓存 ==========
    QMap<QString, DeviceStatus> m_deviceStatusMap;

    // ========== 文件传输状态 ==========
    QString m_currentTaskId;
    QString m_currentTargetDeviceId;
    QFile* m_currentFile;
    qint64 m_fileSize;
    qint64 m_sentBytes;
    QString m_fileSha256;

    // ========== 配置常量 ==========
    static constexpr int SEND_BUFFER_SIZE = 64 * 1024;   // 64KB
    static constexpr int SEND_INTERVAL_MS = 10;          // 10ms
    static constexpr quint16 PACKET_START_MARK = 0x5A5A;
    static constexpr int PACKET_HEADER_SIZE = 9;
};

#endif // DEVICECONNECTOR_H

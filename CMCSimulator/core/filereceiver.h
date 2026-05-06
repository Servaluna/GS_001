#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QCryptographicHash>
#include <QTimer>
#include <QTcpServer>

class Installer;

class FileReceiver : public QObject
{
    Q_OBJECT
public:
    explicit FileReceiver(QObject *parent = nullptr);
    ~FileReceiver();

    bool start(quint16 port);
    void stop();
    bool isReceiving() const;

    void setInstaller(Installer* installer);  // 注入 Installer
    void sendInstallResult(const QString& taskId, bool success, const QString& message);

signals:
    void receiveProgress(QString taskId, qint64 received, qint64 total, int percent);
    void fileReadyForInstall(QString taskId, QString targetDeviceId, QString filePath);
    void receiveFailed(QString taskId, QString errorMessage);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onReceiveTimeout();

private:
    enum class Command : uint8_t {
        FileStart = 0x10,
        FileData = 0x11,
        FileEnd = 0x12,
        FileReceiveResult = 0x03,
        InstallResult = 0x04,
        Error = 0xFF
    };

    struct ReceiveSession {
        QTcpSocket* socket;
        QString taskId;
        QString targetDeviceId;
        QString fileName;
        qint64 fileSize;
        QString expectedSha256;
        qint64 receivedBytes;
        QFile* tempFile;
        QCryptographicHash* hash;
        QTimer* timeoutTimer;
        bool isActive;

        ReceiveSession() : socket(nullptr), fileSize(0), receivedBytes(0),
            tempFile(nullptr), hash(nullptr), timeoutTimer(nullptr),
            isActive(false) {}
    };

    // 数据包处理
    bool parsePacket(const QByteArray& data, Command& cmd, QByteArray& payload);
    QByteArray buildPacket(Command cmd, const QByteArray& payload);

    // 命令处理
    void handleFileStart(ReceiveSession* session, const QByteArray& payload);
    void handleFileData(ReceiveSession* session, const QByteArray& payload);
    void handleFileEnd(ReceiveSession* session);

    // 响应发送
    void sendFileReceiveResult(ReceiveSession* session, bool success, const QString& message);
    void sendError(ReceiveSession* session, const QString& errorMessage);
    void sendInstallResultCommand(ReceiveSession* session, bool success, const QString& message);

    // 会话管理
    ReceiveSession* createSession(QTcpSocket* socket);
    void cleanupSession(ReceiveSession* session);
    ReceiveSession* getSessionBySocket(QTcpSocket* socket);

    // 辅助方法
    QString getTempFilePath(const QString& taskId, const QString& fileName);
    void notifyInstaller(ReceiveSession* session);

private:
    QTcpServer* m_server;
    QMap<QTcpSocket*, ReceiveSession*> m_sessions;

    Installer* m_installer;
    QMap<QString, ReceiveSession*> m_taskToSession;  // taskId -> session

    // 配置
    static constexpr quint16 PACKET_START_MARK = 0x5A5A;
    static constexpr int PACKET_HEADER_SIZE = 9;
    static constexpr int RECEIVE_TIMEOUT_MS = 30000;  // 30秒无数据超时
    static constexpr int MAX_FILE_SIZE = 1024 * 1024 * 500;  // 最大500MB
};

#endif // FILERECEIVER_H

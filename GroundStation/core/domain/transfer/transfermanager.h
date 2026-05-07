#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QObject>
#include <QString>

class DeviceConnector;

class TransferManager : public QObject
{
    Q_OBJECT
public:
    explicit TransferManager(QObject *parent = nullptr);

    bool init(DeviceConnector* deviceConnector);
    bool isDeviceOnline(const QString& deviceId) const;
    void sendFileToDevice(const QString& taskId,
                          const QString& targetDeviceId,
                          const QString& localPath,
                          const QString& fileName,
                          const QString& sha256);

signals:
    void transferProgress(QString taskId, qint64 sent, qint64 total, int percent);
    void sendFinished(QString taskId, bool success, QString message);
    void installResult(QString taskId, QString deviceId, bool success, QString message);

private:
    DeviceConnector* m_deviceConnector;
};

#endif // TRANSFERMANAGER_H

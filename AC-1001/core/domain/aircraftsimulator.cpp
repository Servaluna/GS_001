#include "aircraftsimulator.h"

#include <QCryptographicHash>

AircraftSimulator::AircraftSimulator(QObject *parent)
    : QObject{parent}
    , m_devices({
          {"ADG", "飞机数据网关", "1.0.0", true},
          {"CMC", "中央维护计算机", "1.0.0", true}
      })
{}

QList<AircraftDevice> AircraftSimulator::devices() const
{
    return m_devices;
}

bool AircraftSimulator::validateTargetDevice(const QString& deviceId) const
{
    for (const AircraftDevice& device : m_devices) {
        if (device.deviceId == deviceId && device.online) {
            return true;
        }
    }
    return false;
}

bool AircraftSimulator::verifyPackage(const QByteArray& data, qint64 expectedSize, const QString& expectedSha256) const
{
    if (expectedSize != data.size()) {
        return false;
    }

    if (expectedSha256.isEmpty()) {
        return true;
    }

    const QString actualSha256 = QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    return actualSha256.compare(expectedSha256, Qt::CaseInsensitive) == 0;
}

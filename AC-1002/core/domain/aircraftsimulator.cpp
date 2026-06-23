#include "aircraftsimulator.h"

#include <QCryptographicHash>

AircraftSimulator::AircraftSimulator(QObject *parent)
    : QObject{parent}
    , m_devices({
          {"ADG", "Aircraft Data Gateway", "1.0.0", true},
          {"CMC", "Central Maintenance Computer", "1.0.0", true},
          {"DEV-1002-01", "Mission Computer", "1.0.5", true},
          {"DEV-1002-02", "Flight Control System", "2.0.1", true},
          {"DEV-1002-03", "Data Link Module", "1.0.0", true},
          {"DEV-1002-04", "GPS Sensor", "1.1.0", true}
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

    const QString actualSha256 = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    return actualSha256.compare(expectedSha256, Qt::CaseInsensitive) == 0;
}

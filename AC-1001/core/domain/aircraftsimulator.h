#ifndef AIRCRAFTSIMULATOR_H
#define AIRCRAFTSIMULATOR_H

#include "../models/aircraftdevice.h"

#include <QList>
#include <QObject>

class AircraftSimulator : public QObject
{
    Q_OBJECT

public:
    explicit AircraftSimulator(QObject *parent = nullptr);

    QList<AircraftDevice> devices() const;
    bool validateTargetDevice(const QString& deviceId) const;
    bool verifyPackage(const QByteArray& data, qint64 expectedSize, const QString& expectedSha256) const;

private:
    QList<AircraftDevice> m_devices;
};

#endif // AIRCRAFTSIMULATOR_H

#ifndef AIRCRAFTDEVICE_H
#define AIRCRAFTDEVICE_H

#include <QString>

struct AircraftDevice
{
    QString deviceId;
    QString deviceName;
    QString version;
    bool online = true;
};

#endif // AIRCRAFTDEVICE_H

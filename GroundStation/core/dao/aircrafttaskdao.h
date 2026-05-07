#ifndef AIRCRAFTTASKDAO_H
#define AIRCRAFTTASKDAO_H

#include "../models/aircrafttask.h"

#include <QList>

class AircraftTaskDAO
{
public:
    AircraftTaskDAO() = default;

    bool upsert(const AircraftTask& task) const;
    bool remove(int aircraftTaskId) const;
    AircraftTask getById(int aircraftTaskId) const;
    QList<AircraftTask> getByAssignedOperator(int operatorUserId) const;
    QList<AircraftTask> getAll() const;
    bool updateStatus(int aircraftTaskId, DeviceTaskStatus status, double progress, const QString& phase = QString()) const;
};

#endif // AIRCRAFTTASKDAO_H

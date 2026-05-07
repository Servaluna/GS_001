#ifndef TRANSFERSESSIONDAO_H
#define TRANSFERSESSIONDAO_H

#include "../models/transfersession.h"

#include <QList>

class TransferSessionDAO
{
public:
    TransferSessionDAO() = default;

    bool upsert(const TransferSession& session) const;
    bool remove(const QString& sessionId) const;
    TransferSession getBySessionId(const QString& sessionId) const;
    TransferSession getByDeviceTaskId(int deviceTaskId) const;
    QList<TransferSession> getByStatus(TransferSessionStatus status) const;
    bool updateProgress(const QString& sessionId,
                        qint64 transferredSize,
                        qint64 totalSize,
                        TransferSessionStatus status,
                        const QString& errorMessage = QString()) const;
};

#endif // TRANSFERSESSIONDAO_H

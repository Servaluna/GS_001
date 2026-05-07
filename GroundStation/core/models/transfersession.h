#ifndef TRANSFERSESSION_H
#define TRANSFERSESSION_H

#include "taskstatus.h"

#include <QDateTime>
#include <QJsonObject>
#include <QSqlRecord>
#include <QString>
#include <QVariant>

struct TransferSession
{
    QString transfer_session_id;
    int device_task_id;
    QString device_code;
    QString file_code;
    TransferSessionStatus status;
    double progress;
    QString local_package_path;
    qint64 transferred_size;
    qint64 total_size;
    QString checksum_sha256;
    QDateTime started_at;
    QDateTime updated_at;
    QDateTime finished_at;
    QString error_message;

    TransferSession()
        : device_task_id(-1)
        , status(TransferSessionStatus::Pending)
        , progress(0.0)
        , transferred_size(0)
        , total_size(0)
    {}

    bool isValid() const { return !transfer_session_id.isEmpty() && device_task_id > 0; }

    static TransferSession fromSqlRecord(const QSqlRecord& record)
    {
        TransferSession session;
        session.transfer_session_id = record.value("transfer_session_id").toString();
        session.device_task_id = record.value("device_task_id").toInt();
        session.device_code = record.value("device_code").toString();
        session.file_code = record.value("file_code").toString();
        session.status = TaskStatusText::transferFromInt(record.value("status").toInt());
        session.progress = record.value("progress").toDouble();
        session.local_package_path = record.value("local_package_path").toString();
        session.transferred_size = record.value("transferred_size").toLongLong();
        session.total_size = record.value("total_size").toLongLong();
        session.checksum_sha256 = record.value("checksum_sha256").toString();
        session.started_at = record.value("started_at").toDateTime();
        session.updated_at = record.value("updated_at").toDateTime();
        session.finished_at = record.value("finished_at").toDateTime();
        session.error_message = record.value("error_message").toString();
        return session;
    }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json["transfer_session_id"] = transfer_session_id;
        json["device_task_id"] = device_task_id;
        json["device_code"] = device_code;
        json["file_code"] = file_code;
        json["status"] = TaskStatusText::toInt(status);
        json["progress"] = progress;
        json["local_package_path"] = local_package_path;
        json["transferred_size"] = qint64(transferred_size);
        json["total_size"] = qint64(total_size);
        json["checksum_sha256"] = checksum_sha256;
        json["started_at"] = started_at.toString(Qt::ISODate);
        json["updated_at"] = updated_at.toString(Qt::ISODate);
        json["finished_at"] = finished_at.toString(Qt::ISODate);
        json["error_message"] = error_message;
        return json;
    }
};

#endif // TRANSFERSESSION_H

#include "transfersessiondao.h"

#include "../localdatabase/localdatabase.h"
#include "../logging/logger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

TransferSession transferSessionFromQuery(const QSqlQuery& query)
{
    return TransferSession::fromSqlRecord(query.record());
}

QList<TransferSession> readTransferSessions(QSqlQuery& query)
{
    QList<TransferSession> sessions;
    while (query.next()) {
        sessions.append(transferSessionFromQuery(query));
    }
    return sessions;
}

}

bool TransferSessionDAO::upsert(const TransferSession& session) const
{
    if (!session.isValid()) {
        Logger::warn("DATABASE_SAVE_REJECTED", "无效传输会话，无法保存");
        return false;
    }

    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        INSERT OR REPLACE INTO transfer_session (
            transfer_session_id, device_task_id, device_code, file_code,
            status, progress, local_package_path, transferred_size,
            total_size, checksum_sha256, started_at, updated_at,
            finished_at, error_message
        ) VALUES (
            :transfer_session_id, :device_task_id, :device_code, :file_code,
            :status, :progress, :local_package_path, :transferred_size,
            :total_size, :checksum_sha256, :started_at, :updated_at,
            :finished_at, :error_message
        )
    )");
    query.bindValue(":transfer_session_id", session.transfer_session_id);
    query.bindValue(":device_task_id", session.device_task_id);
    query.bindValue(":device_code", session.device_code);
    query.bindValue(":file_code", session.file_code);
    query.bindValue(":status", TaskStatusText::toInt(session.status));
    query.bindValue(":progress", session.progress);
    query.bindValue(":local_package_path", session.local_package_path);
    query.bindValue(":transferred_size", qint64(session.transferred_size));
    query.bindValue(":total_size", qint64(session.total_size));
    query.bindValue(":checksum_sha256", session.checksum_sha256);
    query.bindValue(":started_at", session.started_at.toString(Qt::ISODate));
    query.bindValue(":updated_at", session.updated_at.toString(Qt::ISODate));
    query.bindValue(":finished_at", session.finished_at.toString(Qt::ISODate));
    query.bindValue(":error_message", session.error_message);

    if (!query.exec()) {
        Logger::error("DATABASE_SAVE_FAILED",
                      "保存传输会话失败",
                      {{"transfer_session_id", session.transfer_session_id}, {"error", query.lastError().text()}});
        return false;
    }
    return true;
}

bool TransferSessionDAO::remove(const QString& sessionId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("DELETE FROM transfer_session WHERE transfer_session_id = :transfer_session_id");
    query.bindValue(":transfer_session_id", sessionId);
    return query.exec();
}

TransferSession TransferSessionDAO::getBySessionId(const QString& sessionId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM transfer_session WHERE transfer_session_id = :transfer_session_id");
    query.bindValue(":transfer_session_id", sessionId);

    if (query.exec() && query.next()) {
        return transferSessionFromQuery(query);
    }
    return TransferSession();
}

TransferSession TransferSessionDAO::getByDeviceTaskId(int deviceTaskId) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        SELECT * FROM transfer_session
        WHERE device_task_id = :device_task_id
        ORDER BY updated_at DESC
        LIMIT 1
    )");
    query.bindValue(":device_task_id", deviceTaskId);

    if (query.exec() && query.next()) {
        return transferSessionFromQuery(query);
    }
    return TransferSession();
}

QList<TransferSession> TransferSessionDAO::getByStatus(TransferSessionStatus status) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare("SELECT * FROM transfer_session WHERE status = :status ORDER BY updated_at DESC");
    query.bindValue(":status", TaskStatusText::toInt(status));

    if (!query.exec()) {
        Logger::error("DATABASE_QUERY_FAILED",
                      "按状态查询传输会话失败",
                      {{"status", TaskStatusText::toInt(status)}, {"error", query.lastError().text()}});
        return {};
    }
    return readTransferSessions(query);
}

bool TransferSessionDAO::updateProgress(const QString& sessionId,
                                        qint64 transferredSize,
                                        qint64 totalSize,
                                        TransferSessionStatus status,
                                        const QString& errorMessage) const
{
    QSqlQuery query(LocalDatabase::getInstance()->getDatabase());
    query.prepare(R"(
        UPDATE transfer_session
        SET transferred_size = :transferred_size,
            total_size = CASE WHEN :total_size > 0 THEN :total_size ELSE total_size END,
            status = :status,
            progress = CASE
                WHEN :total_size > 0 THEN (:transferred_size * 100.0 / :total_size)
                WHEN total_size > 0 THEN (:transferred_size * 100.0 / total_size)
                ELSE progress
            END,
            updated_at = :updated_at,
            finished_at = CASE WHEN :is_finished = 1 THEN :finished_at ELSE finished_at END,
            error_message = :error_message
        WHERE transfer_session_id = :transfer_session_id
    )");
    query.bindValue(":transferred_size", qint64(transferredSize));
    query.bindValue(":total_size", qint64(totalSize));
    query.bindValue(":status", TaskStatusText::toInt(status));
    query.bindValue(":updated_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":is_finished",
                    (status == TransferSessionStatus::Finished ||
                     status == TransferSessionStatus::Failed ||
                     status == TransferSessionStatus::Cancelled) ? 1 : 0);
    query.bindValue(":finished_at", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":error_message", errorMessage);
    query.bindValue(":transfer_session_id", sessionId);
    return query.exec();
}

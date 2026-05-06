#ifndef FILEPROTOCOL_H
#define FILEPROTOCOL_H

#include <QString>
#include <QFile>
#include <QCryptographicHash>

// 文件块大小：4KB（便于演示）
const int CHUNK_SIZE = 4096;

// 计算文件 SHA-256
inline QString calcSha256(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return "";

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    file.close();
    return hash.result().toHex();
}

// 计算数据 SHA-256
inline QString calcDataSha256(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

#endif // FILEPROTOCOL_H

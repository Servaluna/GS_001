#ifndef FILERECEIVERPROTOCOL_H
#define FILERECEIVERPROTOCOL_H

#include <cstdint>

// #pragma pack(push, 1)

/**
 * @brief 地面站 ↔ CMC 通信协议
 * 与地面站 DeviceConnector 中的协议保持一致
 */

// 命令类型
enum class CMCCommand : uint8_t {
    // 地面站 → CMC
    FileStart = 0x10,          // 开始发送文件
    FileData = 0x11,           // 文件数据分片
    FileEnd = 0x12,            // 文件发送完成

    // CMC → 地面站
    FileReceiveResult = 0x03,  // 文件接收结果
    InstallResult = 0x04,      // 设备安装结果
    Error = 0xFF               // 错误
};

// 数据包头部
struct PacketHeader {
    uint16_t startMark;        // 起始标志 0x5A5A
    uint8_t command;           // 命令类型
    uint32_t payloadSize;      // 数据大小
    uint16_t checksum;         // 校验和（异或）
};

// FileStart 命令的数据结构
struct FileStartData {
    uint16_t taskIdSize;
    uint8_t taskId[256];       // 实际大小由 taskIdSize 决定
    uint16_t targetDeviceIdSize;
    uint8_t targetDeviceId[256];
    uint16_t fileNameSize;
    uint8_t fileName[256];
    uint64_t fileSize;
    uint16_t sha256Size;
    uint8_t sha256[64];        // SHA-256 字符串
};

// FileReceiveResult 命令的数据结构
struct FileReceiveResultData {
    uint16_t taskIdSize;
    uint8_t taskId[256];
    uint8_t success;           // 1=成功, 0=失败
    uint16_t messageSize;
    uint8_t message[512];
};

// InstallResult 命令的数据结构
struct InstallResultData {
    uint16_t taskIdSize;
    uint8_t taskId[256];
    uint16_t deviceIdSize;
    uint8_t deviceId[256];
    uint8_t success;           // 1=成功, 0=失败
    uint16_t messageSize;
    uint8_t message[512];
};

// #pragma pack(pop)

#endif // FILERECEIVERPROTOCOL_H

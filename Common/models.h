#ifndef MODELS_H
#define MODELS_H

#include "taskstatus.h"

#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

namespace UserRole {
    enum Role {
        Admin = 0,
        Engineer = 1,
        Operator = 2
    };

    inline Role roleFromString(const QString& role) {
        const QString normalized = role.trimmed().toLower();
        if (normalized == "admin") return Admin;
        if (normalized == "engineer") return Engineer;
        if (normalized == "operator" || normalized == "operater") return Operator;
        return Operator;
    }

    inline QString roleToString(Role role) {
        switch (role) {
        case Admin: return "Admin";
        case Engineer: return "Engineer";
        case Operator: return "Operator";
        default: return "Operator";
        }
    }
}

struct RoleInfo {
    int id;
    QString name;
    QString description;
    QJsonObject permissions;
    QDateTime created_at;

    RoleInfo() : id(-1) {}
    bool isValid() const { return id > 0 && !name.isEmpty(); }
};

struct UserInfo {
    int user_id;
    QString username;
    QString password_hash;
    int role_id;
    QString role;
    int status;
    QDateTime last_login;
    int creator_user_id;
    QDateTime created_at;
    QDateTime updated_at;

    UserInfo()
        : user_id(-1)
        , role_id(-1)
        , status(1)
        , creator_user_id(-1)
    {}

    bool isValid() const { return user_id > 0; }
    bool isActive() const { return status == 1; }
    bool isAdmin() const { return UserRole::roleFromString(role) == UserRole::Admin; }
    bool isEngineer() const { return UserRole::roleFromString(role) == UserRole::Engineer; }
    bool isOperator() const { return UserRole::roleFromString(role) == UserRole::Operator; }

    static UserInfo fromJson(const QJsonObject& json) {
        UserInfo info;
        info.user_id = json["user_id"].toInt(-1);
        info.username = json["username"].toString();
        info.password_hash = json["password_hash"].toString();
        info.role_id = json["role_id"].toInt(-1);
        info.role = json["role"].toString();
        info.status = json["status"].toInt(1);
        info.last_login = QDateTime::fromString(json["last_login"].toString(), Qt::ISODate);
        info.creator_user_id = json["creator_user_id"].toInt(-1);
        info.created_at = QDateTime::fromString(json["created_at"].toString(), Qt::ISODate);
        info.updated_at = QDateTime::fromString(json["updated_at"].toString(), Qt::ISODate);
        return info;
    }

    QJsonObject toJson() const {
        QJsonObject json;
        json["user_id"] = user_id;
        json["username"] = username;
        json["password_hash"] = password_hash;
        json["role_id"] = role_id;
        json["role"] = role;
        json["status"] = status;
        json["last_login"] = last_login.toString(Qt::ISODate);
        json["creator_user_id"] = creator_user_id;
        json["created_at"] = created_at.toString(Qt::ISODate);
        json["updated_at"] = updated_at.toString(Qt::ISODate);
        return json;
    }
};

struct AircraftInfo {
    int aircraft_id;
    QString aircraft_code;
    QString model;
    QString manufacturer;
    QString serial_number;
    int status;
    QString ip_address;
    int port;
    QDateTime created_at;
    QDateTime updated_at;

    AircraftInfo()
        : aircraft_id(-1)
        , status(1)
        , port(0)
    {}

    bool isValid() const { return aircraft_id > 0 || !aircraft_code.isEmpty(); }
    bool isActive() const { return status == 1; }
};

namespace DeviceType {
    enum Type {
        Unknown = 0,
        CentralMaintenanceComputer = 1,
        FlightController = 2,
        DataLink = 3,
        Sensor = 4,
        Payload = 5,
        VideoTransmitter = Payload
    };

    inline Type fromInt(int type) {
        switch (type) {
        case CentralMaintenanceComputer:
        case FlightController:
        case DataLink:
        case Sensor:
        case Payload:
            return static_cast<Type>(type);
        default:
            return Unknown;
        }
    }

    inline Type fromString(const QString& type) {
        if (type == "CentralMaintenanceComputer") return CentralMaintenanceComputer;
        if (type == "FlightController") return FlightController;
        if (type == "DataLink") return DataLink;
        if (type == "Sensor") return Sensor;
        if (type == "Payload" || type == "VideoTransmitter") return Payload;
        return Unknown;
    }

    inline QString toString(Type type) {
        switch (type) {
        case CentralMaintenanceComputer: return "CentralMaintenanceComputer";
        case FlightController: return "FlightController";
        case DataLink: return "DataLink";
        case Sensor: return "Sensor";
        case Payload: return "Payload";
        default: return "Unknown";
        }
    }
}

struct DeviceInfo {
    int device_id;
    QString device_code;
    QString device_name;
    DeviceType::Type device_type;
    QString aircraft_code;
    QString hardware_version;
    QString firmware_version;
    QString software_version;
    int status;
    QJsonObject extra_info;
    QDateTime created_at;
    QDateTime updated_at;

    DeviceInfo()
        : device_id(-1)
        , device_type(DeviceType::Unknown)
        , status(1)
    {}

    bool isValid() const { return device_id > 0 || !device_code.isEmpty(); }
    bool isActive() const { return status == 1; }

    static DeviceInfo fromJson(const QJsonObject& json) {
        DeviceInfo info;
        info.device_id = json["device_id"].toInt(-1);
        info.device_code = json["device_code"].toString();
        info.device_name = json["device_name"].toString();
        if (json["device_type"].isDouble()) {
            info.device_type = DeviceType::fromInt(json["device_type"].toInt());
        } else {
            info.device_type = DeviceType::fromString(json["device_type"].toString());
        }
        info.aircraft_code = json["aircraft_code"].toString();
        info.hardware_version = json["hardware_version"].toString();
        info.firmware_version = json["firmware_version"].toString();
        info.software_version = json["software_version"].toString();
        info.status = json["status"].toInt(1);
        info.extra_info = json["extra_info"].toObject();
        info.created_at = QDateTime::fromString(json["created_at"].toString(), Qt::ISODate);
        info.updated_at = QDateTime::fromString(json["updated_at"].toString(), Qt::ISODate);
        return info;
    }

    QJsonObject toJson() const {
        QJsonObject json;
        json["device_id"] = device_id;
        json["device_code"] = device_code;
        json["device_name"] = device_name;
        json["device_type"] = static_cast<int>(device_type);
        json["aircraft_code"] = aircraft_code;
        json["hardware_version"] = hardware_version;
        json["firmware_version"] = firmware_version;
        json["software_version"] = software_version;
        json["status"] = status;
        json["extra_info"] = extra_info;
        json["created_at"] = created_at.toString(Qt::ISODate);
        json["updated_at"] = updated_at.toString(Qt::ISODate);
        return json;
    }
};

namespace FileType {
    enum Type {
        Unknown = 0,
        Firmware = 1,
        Software = 2,
        Config = 3,
        Script = 4,
        Log = 5
    };

    inline Type fromInt(int type) {
        switch (type) {
        case Firmware:
        case Software:
        case Config:
        case Script:
        case Log:
            return static_cast<Type>(type);
        default:
            return Unknown;
        }
    }

    inline Type fromString(const QString& type) {
        if (type == "Firmware") return Firmware;
        if (type == "Software") return Software;
        if (type == "Config") return Config;
        if (type == "Script") return Script;
        if (type == "Log") return Log;
        return Unknown;
    }

    inline QString toString(Type type) {
        switch (type) {
        case Firmware: return "Firmware";
        case Software: return "Software";
        case Config: return "Config";
        case Script: return "Script";
        case Log: return "Log";
        default: return "Unknown";
        }
    }
}

struct FileInfo {
    int file_id;
    QString file_code;
    QString file_name;
    FileType::Type file_type;
    qint64 file_size;
    QString sha256_hash;
    QString storage_path;
    QString version;
    QString description;
    int status;
    int uploader_user_id;
    QDateTime created_at;
    QDateTime updated_at;

    FileInfo()
        : file_id(-1)
        , file_type(FileType::Unknown)
        , file_size(0)
        , status(1)
        , uploader_user_id(-1)
    {}

    bool isValid() const { return (file_id > 0 || !file_code.isEmpty()) && file_size > 0; }
    bool isActive() const { return status == 1; }
    bool isFirmware() const { return file_type == FileType::Firmware; }
    bool isConfig() const { return file_type == FileType::Config; }

    QString getFileSizeDisplay() const {
        if (file_size < 1024) return QString("%1 B").arg(file_size);
        if (file_size < 1024 * 1024) return QString("%1 KB").arg(file_size / 1024.0, 0, 'f', 1);
        if (file_size < 1024 * 1024 * 1024) return QString("%1 MB").arg(file_size / (1024.0 * 1024.0), 0, 'f', 1);
        return QString("%1 GB").arg(file_size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }

    static FileInfo fromJson(const QJsonObject& json) {
        FileInfo info;
        info.file_id = json["file_id"].toInt(-1);
        info.file_code = json["file_code"].toString();
        info.file_name = json["file_name"].toString();
        if (json["file_type"].isDouble()) {
            info.file_type = FileType::fromInt(json["file_type"].toInt());
        } else {
            info.file_type = FileType::fromString(json["file_type"].toString());
        }
        info.file_size = json["file_size"].toInteger();
        info.sha256_hash = json["sha256_hash"].toString();
        info.storage_path = json["storage_path"].toString();
        info.version = json["version"].toString();
        info.description = json["description"].toString();
        info.status = json["status"].toInt(1);
        info.uploader_user_id = json["uploader_user_id"].toInt(-1);
        info.created_at = QDateTime::fromString(json["created_at"].toString(), Qt::ISODate);
        info.updated_at = QDateTime::fromString(json["updated_at"].toString(), Qt::ISODate);
        return info;
    }

    QJsonObject toJson() const {
        QJsonObject json;
        json["file_id"] = file_id;
        json["file_code"] = file_code;
        json["file_name"] = file_name;
        json["file_type"] = static_cast<int>(file_type);
        json["file_size"] = qint64(file_size);
        json["sha256_hash"] = sha256_hash;
        json["storage_path"] = storage_path;
        json["version"] = version;
        json["description"] = description;
        json["status"] = status;
        json["uploader_user_id"] = uploader_user_id;
        json["created_at"] = created_at.toString(Qt::ISODate);
        json["updated_at"] = updated_at.toString(Qt::ISODate);
        return json;
    }
};

struct BatchUpgradeTaskInfo {
    int batch_id;
    QString batch_name;
    QString description;
    int creator_user_id;
    BatchTaskStatus status;
    QDateTime start_time;
    QDateTime finish_time;
    QDateTime create_at;
    QDateTime updated_at;

    BatchUpgradeTaskInfo()
        : batch_id(-1)
        , creator_user_id(-1)
        , status(BatchTaskStatus::Created)
    {}

    bool isValid() const { return batch_id > 0; }
};

namespace TaskType {
    enum Type {
        Unknown = 0,
        Upgrade = 1,
        Config = 2,
        LogDownload = 3,
        Diagnostic = 4
    };

    inline Type fromString(const QString& type) {
        if (type == "Upgrade") return Upgrade;
        if (type == "Config") return Config;
        if (type == "LogDownload") return LogDownload;
        if (type == "Diagnostic") return Diagnostic;
        return Unknown;
    }

    inline QString toString(Type type) {
        switch (type) {
        case Upgrade: return "Upgrade";
        case Config: return "Config";
        case LogDownload: return "LogDownload";
        case Diagnostic: return "Diagnostic";
        default: return "Unknown";
        }
    }
}

struct AircraftUpgradeTaskInfo {
    int aircraft_task_id;
    int batch_id;
    QString aircraft_code;
    int assigned_operator_user_id;
    DeviceTaskStatus status;
    double progress;
    QString current_phase;
    QString current_client_id;
    QDateTime start_time;
    QDateTime last_update_time;
    QDateTime finish_time;
    QString last_error;

    AircraftUpgradeTaskInfo()
        : aircraft_task_id(-1)
        , batch_id(-1)
        , assigned_operator_user_id(-1)
        , status(DeviceTaskStatus::Waiting)
        , progress(0.0)
    {}

    bool isValid() const { return aircraft_task_id > 0; }
};

struct DeviceUpgradeTaskInfo {
    int device_task_id;
    int aircraft_task_id;
    QString device_code;
    QString file_code;
    int execution_order;
    DeviceTaskStatus status;
    double progress;
    int retry_count;
    qint64 transferred_size;
    qint64 total_size;
    QDateTime start_time;
    QDateTime last_update_time;
    QDateTime finish_time;
    QString last_error;

    DeviceUpgradeTaskInfo()
        : device_task_id(-1)
        , aircraft_task_id(-1)
        , execution_order(0)
        , status(DeviceTaskStatus::Waiting)
        , progress(0.0)
        , retry_count(0)
        , transferred_size(0)
        , total_size(0)
    {}

    bool isValid() const { return device_task_id > 0; }
};

struct AuditLogInfo {
    int log_id;
    QString event_type;
    QString event_level;
    int operator_user_id;
    QString client_machine_id;
    QString session_id;
    int batch_id;
    int aircraft_task_id;
    int device_task_id;
    QString aircraft_code;
    QString device_code;
    QString file_code;
    QString event_message;
    QJsonObject event_detail;
    QString ip_address;
    QDateTime create_at;

    AuditLogInfo()
        : log_id(-1)
        , operator_user_id(-1)
        , batch_id(-1)
        , aircraft_task_id(-1)
        , device_task_id(-1)
    {}

    bool isValid() const { return log_id > 0; }
};

struct TaskBasicInfo {
    QString task_id;
    TaskType::Type task_type;
    QString description;
    QString file_code;
    QString aircraft_code;
    QString device_code;
    int priority;
    DeviceTaskStatus status;
    QDateTime create_time;
    QDateTime start_time;
    QDateTime end_time;
    QString creator;
    QJsonObject parameters;

    TaskBasicInfo()
        : task_type(TaskType::Unknown)
        , priority(5)
        , status(DeviceTaskStatus::Waiting)
    {}

    bool isValid() const {
        return !task_id.isEmpty() && task_type != TaskType::Unknown;
    }

    QString getTypeDisplayName() const {
        switch (task_type) {
        case TaskType::Upgrade: return "固件升级";
        case TaskType::Config: return "配置上传";
        case TaskType::LogDownload: return "日志下载";
        case TaskType::Diagnostic: return "诊断测试";
        default: return "未知任务";
        }
    }

    static TaskBasicInfo fromJson(const QJsonObject& json) {
        TaskBasicInfo info;
        info.task_id = json["task_id"].toString();
        info.task_type = TaskType::fromString(json["task_type"].toString());
        info.description = json["description"].toString();
        info.file_code = json["file_code"].toString();
        info.aircraft_code = json["aircraft_code"].toString();
        info.device_code = json["device_code"].toString();
        info.priority = json["priority"].toInt(5);
        info.status = TaskStatusText::deviceFromInt(json["status"].toInt());
        info.create_time = QDateTime::fromString(json["create_time"].toString(), Qt::ISODate);
        info.start_time = QDateTime::fromString(json["start_time"].toString(), Qt::ISODate);
        info.end_time = QDateTime::fromString(json["end_time"].toString(), Qt::ISODate);
        info.creator = json["creator"].toString();
        info.parameters = json["parameters"].toObject();
        return info;
    }

    QJsonObject toJson() const {
        QJsonObject json;
        json["task_id"] = task_id;
        json["task_type"] = TaskType::toString(task_type);
        json["description"] = description;
        json["file_code"] = file_code;
        json["aircraft_code"] = aircraft_code;
        json["device_code"] = device_code;
        json["priority"] = priority;
        json["status"] = TaskStatusText::toInt(status);
        json["create_time"] = create_time.toString(Qt::ISODate);
        json["start_time"] = start_time.toString(Qt::ISODate);
        json["end_time"] = end_time.toString(Qt::ISODate);
        json["creator"] = creator;
        json["parameters"] = parameters;
        return json;
    }
};

#endif // MODELS_H

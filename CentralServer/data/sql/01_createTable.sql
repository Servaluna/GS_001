CREATE TABLE IF NOT EXISTS `roles` (
    `id` INT PRIMARY KEY AUTO_INCREMENT,
    `name` VARCHAR(50) UNIQUE NOT NULL COMMENT 'Admin, Engineer, Operator',
    `description` VARCHAR(255),
    `permissions` JSON DEFAULT NULL,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色信息表';

CREATE TABLE IF NOT EXISTS `users` (
    `user_id` INT PRIMARY KEY AUTO_INCREMENT,
    `username` VARCHAR(64) UNIQUE NOT NULL,
    `password_hash` VARCHAR(255) NOT NULL,
    `role_id` INT NOT NULL,
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '0-无效, 1-有效',
    `last_login` DATETIME DEFAULT NULL,
    `creator_user_id` INT DEFAULT NULL,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    INDEX `idx_users_role_id` (`role_id`),
    INDEX `idx_users_creator` (`creator_user_id`),

    FOREIGN KEY (`role_id`) REFERENCES `roles`(`id`),
    FOREIGN KEY (`creator_user_id`) REFERENCES `users`(`user_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户信息表';

CREATE TABLE IF NOT EXISTS `aircrafts` (
    `aircraft_id` INT PRIMARY KEY AUTO_INCREMENT,
    `aircraft_code` VARCHAR(64) NOT NULL UNIQUE,
    `model` VARCHAR(128) NOT NULL,
    `manufacturer` VARCHAR(128) NOT NULL,
    `serial_number` VARCHAR(64) NOT NULL,
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '0-废弃, 1-使用',
    `ip_address` VARCHAR(45) NOT NULL DEFAULT '',
    `port` INT NOT NULL DEFAULT 0,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    INDEX `idx_aircraft_code` (`aircraft_code`),
    INDEX `idx_aircraft_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='飞机信息表';

CREATE TABLE IF NOT EXISTS `devices` (
    `device_id` INT PRIMARY KEY AUTO_INCREMENT,
    `device_code` VARCHAR(64) NOT NULL UNIQUE,
    `device_name` VARCHAR(128) NOT NULL,
    `device_type` TINYINT NOT NULL DEFAULT 0 COMMENT '0-未知, 1-任务计算机, 2-飞控, 3-数传, 4-传感器, 5-载荷',
    `aircraft_id` INT NOT NULL,
    `hardware_version` VARCHAR(32) DEFAULT '',
    `firmware_version` VARCHAR(32) DEFAULT '',
    `software_version` VARCHAR(32) DEFAULT '',
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '0-废弃, 1-使用',
    `extra_info` JSON DEFAULT NULL,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    INDEX `idx_device_code` (`device_code`),
    INDEX `idx_devices_aircraft_id` (`aircraft_id`),
    INDEX `idx_devices_status` (`status`),
    INDEX `idx_devices_type` (`device_type`),

    FOREIGN KEY (`aircraft_id`) REFERENCES `aircrafts`(`aircraft_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='设备信息表';

CREATE TABLE IF NOT EXISTS `files` (
    `file_id` INT PRIMARY KEY AUTO_INCREMENT,
    `file_code` VARCHAR(64) NOT NULL UNIQUE,
    `file_name` VARCHAR(255) NOT NULL,
    `file_type` TINYINT NOT NULL DEFAULT 0 COMMENT '0-未知, 1-固件, 2-软件, 3-配置, 4-脚本, 5-日志',
    `file_size` BIGINT NOT NULL DEFAULT 0,
    `sha256_hash` VARCHAR(64) NOT NULL COMMENT 'SHA-256 哈希值，64 位十六进制',
    `storage_path` VARCHAR(512) NOT NULL,
    `version` VARCHAR(32) DEFAULT '',
    `description` TEXT,
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '0-无效, 1-有效',
    `uploader_user_id` INT DEFAULT NULL,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    INDEX `idx_file_code` (`file_code`),
    INDEX `idx_files_uploader` (`uploader_user_id`),
    INDEX `idx_files_status` (`status`),
    INDEX `idx_sha256_hash` (`sha256_hash`),

    FOREIGN KEY (`uploader_user_id`) REFERENCES `users`(`user_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='文件信息表';

CREATE TABLE IF NOT EXISTS `batch_upgrade_task` (
    `batch_id` INT PRIMARY KEY AUTO_INCREMENT,
    `batch_name` VARCHAR(128) NOT NULL,
    `description` TEXT,
    `creator_user_id` INT DEFAULT NULL,
    `status` TINYINT NOT NULL DEFAULT 0 COMMENT '0-CREATED, 1-RUNNING, 2-FINISHED, 3-FAILED, 4-CANCELLED',
    `start_time` DATETIME DEFAULT NULL,
    `finish_time` DATETIME DEFAULT NULL,
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    INDEX `idx_batch_creator` (`creator_user_id`),
    INDEX `idx_batch_status` (`status`),

    FOREIGN KEY (`creator_user_id`) REFERENCES `users`(`user_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='升级批次表';

CREATE TABLE IF NOT EXISTS `aircraft_upgrade_task` (
    `aircraft_task_id` INT PRIMARY KEY AUTO_INCREMENT,
    `batch_id` INT NOT NULL,
    `aircraft_id` INT NOT NULL,
    `assigned_operator_user_id` INT DEFAULT NULL,
    `status` TINYINT NOT NULL DEFAULT 0 COMMENT '0-WAITING, 1-DOWNLOADING, 2-TRANSFERRING, 3-INSTALLING, 4-VERIFYING, 5-SUCCESS, 6-FAILED',
    `progress` FLOAT NOT NULL DEFAULT 0,
    `current_phase` VARCHAR(64),
    `current_client_id` VARCHAR(128),
    `start_time` DATETIME DEFAULT NULL,
    `last_update_time` DATETIME DEFAULT NULL,
    `finish_time` DATETIME DEFAULT NULL,
    `last_error` TEXT,

    INDEX `idx_aircraft_task_batch` (`batch_id`),
    INDEX `idx_aircraft_task_aircraft` (`aircraft_id`),
    INDEX `idx_assigned_operator` (`assigned_operator_user_id`),
    INDEX `idx_aircraft_task_status` (`status`),

    FOREIGN KEY (`batch_id`) REFERENCES `batch_upgrade_task`(`batch_id`) ON DELETE CASCADE,
    FOREIGN KEY (`aircraft_id`) REFERENCES `aircrafts`(`aircraft_id`) ON DELETE CASCADE,
    FOREIGN KEY (`assigned_operator_user_id`) REFERENCES `users`(`user_id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='飞机升级任务表';

CREATE TABLE IF NOT EXISTS `device_upgrade_task` (
    `device_task_id` INT PRIMARY KEY AUTO_INCREMENT,
    `aircraft_task_id` INT NOT NULL,
    `device_id` INT NOT NULL,
    `file_id` INT NOT NULL,
    `execution_order` INT NOT NULL DEFAULT 0,
    `status` TINYINT NOT NULL DEFAULT 0 COMMENT '0-WAITING, 1-DOWNLOADING, 2-TRANSFERRING, 3-INSTALLING, 4-VERIFYING, 5-SUCCESS, 6-FAILED',
    `progress` FLOAT NOT NULL DEFAULT 0,
    `retry_count` INT NOT NULL DEFAULT 0,
    `transferred_size` BIGINT NOT NULL DEFAULT 0,
    `total_size` BIGINT DEFAULT NULL,
    `start_time` DATETIME DEFAULT NULL,
    `last_update_time` DATETIME DEFAULT NULL,
    `finish_time` DATETIME DEFAULT NULL,
    `last_error` TEXT,

    INDEX `idx_device_task_aircraft_task` (`aircraft_task_id`),
    INDEX `idx_device_task_device` (`device_id`),
    INDEX `idx_device_task_file` (`file_id`),
    INDEX `idx_device_task_status` (`status`),

    FOREIGN KEY (`aircraft_task_id`) REFERENCES `aircraft_upgrade_task`(`aircraft_task_id`) ON DELETE CASCADE,
    FOREIGN KEY (`device_id`) REFERENCES `devices`(`device_id`) ON DELETE CASCADE,
    FOREIGN KEY (`file_id`) REFERENCES `files`(`file_id`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='设备升级任务表';

CREATE TABLE IF NOT EXISTS `audit_log` (
    `log_id` INT PRIMARY KEY AUTO_INCREMENT,
    `event_type` VARCHAR(64) NOT NULL,
    `event_level` VARCHAR(16) NOT NULL DEFAULT 'INFO',
    `operator_user_id` INT DEFAULT NULL,
    `client_machine_id` VARCHAR(128) DEFAULT '',
    `session_id` VARCHAR(128) DEFAULT '',
    `batch_id` INT DEFAULT NULL,
    `aircraft_task_id` INT DEFAULT NULL,
    `device_task_id` INT DEFAULT NULL,
    `aircraft_id` INT DEFAULT NULL,
    `device_id` INT DEFAULT NULL,
    `file_id` INT DEFAULT NULL,
    `event_message` TEXT,
    `event_detail` JSON DEFAULT NULL,
    `ip_address` VARCHAR(64),
    `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,

    INDEX `idx_audit_created_at` (`created_at`),
    INDEX `idx_audit_event_type` (`event_type`),
    INDEX `idx_audit_operator` (`operator_user_id`),
    INDEX `idx_audit_task` (`aircraft_task_id`, `device_task_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='统一审计日志表';
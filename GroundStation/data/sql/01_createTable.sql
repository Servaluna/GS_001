-- GroundStation SQLite 本地任务库
-- 三层结构：
-- 1. device_upgrade_task：设备升级任务生命周期
-- 2. download_session：文件下载生命周期
-- 3. transfer_session：文件传输/安装生命周期

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS device_upgrade_task (
    device_task_id INTEGER PRIMARY KEY,

    server_device_task_id INTEGER,
    aircraft_task_id INTEGER,
    batch_id INTEGER,

    owner_user_id INTEGER NOT NULL,
    assigned_operator_user_id INTEGER,

    aircraft_code TEXT,
    device_code TEXT NOT NULL,
    file_code TEXT NOT NULL,
    execution_order INTEGER DEFAULT 0,

    status INTEGER DEFAULT 0,          -- 0-WAITING 1-DOWNLOADING 2-TRANSFERRING 3-INSTALLING 4-VERIFYING 5-SUCCESS 6-FAILED
    progress REAL DEFAULT 0,
    current_phase TEXT,                -- waiting/downloading/transferring/installing/verifying/success/failed
    retry_count INTEGER DEFAULT 0,

    local_package_path TEXT,

    total_size INTEGER DEFAULT 0,
    downloaded_size INTEGER DEFAULT 0,
    transferred_size INTEGER DEFAULT 0,

    start_time DATETIME,
    last_update_time DATETIME,
    finish_time DATETIME,
    last_error TEXT
);

CREATE TABLE IF NOT EXISTS download_session (
    download_session_id TEXT PRIMARY KEY,

    device_task_id INTEGER NOT NULL,
    file_code TEXT NOT NULL,

    status INTEGER DEFAULT 0,          -- 0-待下载 1-下载中 2-已完成 3-失败 4-已暂停 5-已取消
    progress REAL DEFAULT 0,

    local_path TEXT,
    temp_path TEXT,
    downloaded_size INTEGER DEFAULT 0,
    total_size INTEGER DEFAULT 0,
    checksum_sha256 TEXT,

    started_at DATETIME,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    finished_at DATETIME,
    expire_time DATETIME,
    error_message TEXT,

    FOREIGN KEY (device_task_id) REFERENCES device_upgrade_task(device_task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS transfer_session (
    transfer_session_id TEXT PRIMARY KEY,

    device_task_id INTEGER NOT NULL,
    device_code TEXT NOT NULL,
    file_code TEXT NOT NULL,

    status INTEGER DEFAULT 0,          -- 0-待传输 1-传输中 2-已完成 3-失败 4-已暂停 5-已取消
    progress REAL DEFAULT 0,

    local_package_path TEXT,
    transferred_size INTEGER DEFAULT 0,
    total_size INTEGER DEFAULT 0,
    checksum_sha256 TEXT,

    started_at DATETIME,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    finished_at DATETIME,
    error_message TEXT,

    FOREIGN KEY (device_task_id) REFERENCES device_upgrade_task(device_task_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_operator
    ON device_upgrade_task(assigned_operator_user_id);

CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_aircraft
    ON device_upgrade_task(aircraft_task_id);

CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_status
    ON device_upgrade_task(status);

CREATE INDEX IF NOT EXISTS idx_device_upgrade_task_order
    ON device_upgrade_task(aircraft_task_id, execution_order);

CREATE INDEX IF NOT EXISTS idx_download_session_device_task
    ON download_session(device_task_id);

CREATE INDEX IF NOT EXISTS idx_download_session_status
    ON download_session(status);

CREATE INDEX IF NOT EXISTS idx_transfer_session_device_task
    ON transfer_session(device_task_id);

CREATE INDEX IF NOT EXISTS idx_transfer_session_status
    ON transfer_session(status);

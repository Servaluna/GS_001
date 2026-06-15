-- GroundStation SQLite 本地测试数据重置脚本
-- 用途：清理本地下载/传输会话，并把本地缓存任务恢复到“待执行”。
-- 注意：GS 登录后会从 CentralServer 重新同步任务，本脚本只处理本地缓存库。

PRAGMA foreign_keys = ON;

DELETE FROM transfer_session;
DELETE FROM download_session;

UPDATE device_upgrade_task
SET status = 0,
    progress = 0,
    current_phase = 'waiting',
    retry_count = 0,
    local_package_path = NULL,
    downloaded_size = 0,
    transferred_size = 0,
    start_time = NULL,
    last_update_time = NULL,
    finish_time = NULL,
    last_error = NULL;

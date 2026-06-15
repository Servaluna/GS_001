-- CentralServer MySQL 测试任务重置脚本
-- 用途：每次手动测试前/后执行，让任务回到“待执行/未开始”状态。
-- 注意：current_phase 统一使用英文阶段码：
-- waiting / downloading / transferring / installing / verifying / success / failed
--
-- 建议执行顺序：
-- 1. 停止正在执行任务的 GroundStation
-- 2. 执行本脚本
-- 3. 如需清空 GS 本地缓存，再执行 GroundStation/data/sql/99_resetLocalTaskTestData.sql
-- 4. 重新启动 GroundStation 登录测试

USE centralserver;

UPDATE batch_upgrade_task
SET status = 0,
    start_time = NULL,
    finish_time = NULL
WHERE batch_id > 0;

UPDATE aircraft_upgrade_task
SET status = 0,
    progress = 0,
    current_phase = 'waiting',
    current_client_id = NULL,
    start_time = NULL,
    last_update_time = NULL,
    finish_time = NULL,
    last_error = NULL
WHERE aircraft_task_id > 0;

UPDATE device_upgrade_task
SET status = 0,
    progress = 0,
    retry_count = 0,
    transferred_size = 0,
    start_time = NULL,
    last_update_time = NULL,
    finish_time = NULL,
    last_error = NULL
WHERE device_task_id > 0;

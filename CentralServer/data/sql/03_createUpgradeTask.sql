SET @engineer_id = (SELECT user_id FROM users WHERE username = '工程师1' LIMIT 1);
SET @operator1_id = (SELECT user_id FROM users WHERE username = '操作员1' LIMIT 1);
SET @operator2_id = (SELECT user_id FROM users WHERE username = '操作员2' LIMIT 1);

INSERT INTO batch_upgrade_task (batch_name, description, creator_user_id, status, start_time, finish_time) VALUES
('2026年第一季度固件批量升级', '对 AC-1001、AC-1002 的多个设备进行固件和软件升级，提升系统稳定性', @engineer_id, 0, NULL, NULL),
('飞控系统安全补丁升级', '针对 AC-1003、AC-1004、AC-1005 的飞控和安全系统进行重要补丁升级', @engineer_id, 0, NULL, NULL);

SET @batch1_id = (SELECT batch_id FROM batch_upgrade_task WHERE batch_name = '2026年第一季度固件批量升级' LIMIT 1);
SET @batch2_id = (SELECT batch_id FROM batch_upgrade_task WHERE batch_name = '飞控系统安全补丁升级' LIMIT 1);

SET @aircraft_ac1001_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1001' LIMIT 1);
SET @aircraft_ac1002_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1002' LIMIT 1);
SET @aircraft_ac1003_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1003' LIMIT 1);
SET @aircraft_ac1004_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1004' LIMIT 1);
SET @aircraft_ac1005_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1005' LIMIT 1);

INSERT INTO aircraft_upgrade_task (batch_id, aircraft_id, assigned_operator_user_id, status, progress, current_phase) VALUES
(@batch1_id, @aircraft_ac1001_id, @operator1_id, 0, 0, 'waiting'),
(@batch1_id, @aircraft_ac1002_id, @operator1_id, 0, 0, 'waiting'),
(@batch2_id, @aircraft_ac1003_id, @operator2_id, 0, 0, 'waiting'),
(@batch2_id, @aircraft_ac1004_id, @operator2_id, 0, 0, 'waiting'),
(@batch2_id, @aircraft_ac1005_id, @operator2_id, 0, 0, 'waiting');

SET @ac_task1_id = (SELECT aircraft_task_id FROM aircraft_upgrade_task WHERE batch_id = @batch1_id AND aircraft_id = @aircraft_ac1001_id LIMIT 1);
SET @ac_task2_id = (SELECT aircraft_task_id FROM aircraft_upgrade_task WHERE batch_id = @batch1_id AND aircraft_id = @aircraft_ac1002_id LIMIT 1);
SET @ac_task3_id = (SELECT aircraft_task_id FROM aircraft_upgrade_task WHERE batch_id = @batch2_id AND aircraft_id = @aircraft_ac1003_id LIMIT 1);
SET @ac_task4_id = (SELECT aircraft_task_id FROM aircraft_upgrade_task WHERE batch_id = @batch2_id AND aircraft_id = @aircraft_ac1004_id LIMIT 1);
SET @ac_task5_id = (SELECT aircraft_task_id FROM aircraft_upgrade_task WHERE batch_id = @batch2_id AND aircraft_id = @aircraft_ac1005_id LIMIT 1);

SET @file_fw001_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-001' LIMIT 1);
SET @file_fw002_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-002' LIMIT 1);
SET @file_fw003_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-003' LIMIT 1);
SET @file_sw001_id = (SELECT file_id FROM files WHERE file_code = 'FILE-SW-001' LIMIT 1);
SET @file_fw004_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-004' LIMIT 1);
SET @file_fw005_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-005' LIMIT 1);
SET @file_fw006_id = (SELECT file_id FROM files WHERE file_code = 'FILE-FW-006' LIMIT 1);
SET @file_sw002_id = (SELECT file_id FROM files WHERE file_code = 'FILE-SW-002' LIMIT 1);

INSERT INTO device_upgrade_task (aircraft_task_id, device_id, file_id, execution_order, status, progress, total_size) VALUES
(@ac_task1_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1001-01' LIMIT 1), @file_fw001_id, 1, 0, 0, 15728640),
(@ac_task1_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1001-01' LIMIT 1), @file_sw001_id, 2, 0, 0, 5242880),
(@ac_task1_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1001-02' LIMIT 1), @file_fw002_id, 3, 0, 0, 8388608),
(@ac_task1_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1001-03' LIMIT 1), @file_fw003_id, 4, 0, 0, 2097152),
(@ac_task1_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1001-04' LIMIT 1), @file_fw005_id, 5, 0, 0, 1048576),

(@ac_task2_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1002-02' LIMIT 1), @file_fw004_id, 1, 0, 0, 9437184),
(@ac_task2_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1002-01' LIMIT 1), @file_fw001_id, 2, 0, 0, 15728640),
(@ac_task2_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1002-03' LIMIT 1), @file_fw003_id, 3, 0, 0, 2097152),
(@ac_task2_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1002-04' LIMIT 1), @file_sw002_id, 4, 0, 0, 8388608),

(@ac_task3_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1003-02' LIMIT 1), @file_fw002_id, 1, 0, 0, 8388608),
(@ac_task3_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1003-01' LIMIT 1), @file_sw001_id, 2, 0, 0, 5242880),
(@ac_task3_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1003-03' LIMIT 1), @file_fw003_id, 3, 0, 0, 2097152),

(@ac_task4_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1004-01' LIMIT 1), @file_fw004_id, 1, 0, 0, 9437184),
(@ac_task4_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1004-02' LIMIT 1), @file_fw003_id, 2, 0, 0, 2097152),
(@ac_task4_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1004-03' LIMIT 1), @file_fw006_id, 3, 0, 0, 5242880),

(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-02' LIMIT 1), @file_fw004_id, 1, 0, 0, 9437184),
(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-01' LIMIT 1), @file_fw001_id, 2, 0, 0, 15728640),
(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-01' LIMIT 1), @file_sw001_id, 3, 0, 0, 5242880),
(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-03' LIMIT 1), @file_fw003_id, 4, 0, 0, 2097152),
(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-04' LIMIT 1), @file_sw002_id, 5, 0, 0, 8388608),
(@ac_task5_id, (SELECT device_id FROM devices WHERE device_code = 'DEV-1005-05' LIMIT 1), @file_fw005_id, 6, 0, 0, 1048576);

INSERT INTO audit_log (event_type, event_level, operator_user_id, client_machine_id, session_id, batch_id, event_message, ip_address) VALUES
('BATCH_CREATE', 'INFO', @engineer_id, 'SERVER-001', CONCAT('SESSION-', UNIX_TIMESTAMP()), @batch1_id, '创建升级批次：2026年第一季度固件批量升级', '127.0.0.1'),
('BATCH_CREATE', 'INFO', @engineer_id, 'SERVER-001', CONCAT('SESSION-', UNIX_TIMESTAMP() + 1), @batch2_id, '创建升级批次：飞控系统安全补丁升级', '127.0.0.1');

INSERT INTO audit_log (event_type, event_level, operator_user_id, client_machine_id, session_id, batch_id, aircraft_task_id, event_message, ip_address)
SELECT 'TASK_ASSIGN', 'INFO', @engineer_id, 'SERVER-001', CONCAT('SESSION-', UNIX_TIMESTAMP()), batch_id, aircraft_task_id,
       CONCAT('分配飞机升级任务: ', aircraft_id, ' 给操作员'), '127.0.0.1'
FROM aircraft_upgrade_task
WHERE batch_id IN (@batch1_id, @batch2_id);

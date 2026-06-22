INSERT INTO roles (name, description) VALUES
('Admin', '管理员：系统管理和用户管理'),
('Engineer', '工程师：上传管理升级包和创建升级任务'),
('Operator', '操作员：执行常规升级任务');

SET @admin_role_id = (SELECT id FROM roles WHERE name = 'Admin' LIMIT 1);
SET @engineer_role_id = (SELECT id FROM roles WHERE name = 'Engineer' LIMIT 1);
SET @operator_role_id = (SELECT id FROM roles WHERE name = 'Operator' LIMIT 1);

INSERT INTO users (username, password_hash, role_id, creator_user_id) VALUES
('root', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', @admin_role_id, NULL);

UPDATE users SET creator_user_id = user_id WHERE username = 'root';
SET @root_id = (SELECT user_id FROM users WHERE username = 'root' LIMIT 1);

INSERT INTO users (username, password_hash, role_id, creator_user_id) VALUES
('管理员1', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', @admin_role_id, @root_id),
('工程师1', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', @engineer_role_id, @root_id),
('操作员1', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', @operator_role_id, @root_id),
('操作员2', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', @operator_role_id, @root_id);

INSERT INTO aircrafts (aircraft_code, model, manufacturer, serial_number, status, ip_address, port) VALUES
('AC-1001', 'Boeing 737-800', 'Boeing', 'SN-737-00123', 1, '192.168.1.101', 8080),
('AC-1002', 'Airbus A320', 'Airbus', 'SN-320-00456', 1, '192.168.1.102', 8080),
('AC-1003', 'Cessna 172', 'Textron Aviation', 'SN-172-00789', 1, '192.168.1.103', 8080),
('AC-1004', 'DJI M300', 'DJI', 'SN-DJI-00112', 1, '192.168.1.104', 8080),
('AC-1005', 'Boeing 787 Dreamliner', 'Boeing', 'SN-787-00345', 1, '192.168.1.105', 8080);

SET @aircraft_ac1001_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1001' LIMIT 1);
SET @aircraft_ac1002_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1002' LIMIT 1);
SET @aircraft_ac1003_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1003' LIMIT 1);
SET @aircraft_ac1004_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1004' LIMIT 1);
SET @aircraft_ac1005_id = (SELECT aircraft_id FROM aircrafts WHERE aircraft_code = 'AC-1005' LIMIT 1);

INSERT INTO devices (device_code, device_name, device_type, aircraft_id, hardware_version, firmware_version, software_version, status) VALUES
('DEV-1001-01', '任务计算机', 1, @aircraft_ac1001_id, 'v2.0', 'v1.2.0', 'v1.0.5', 1),
('DEV-1001-02', '飞控系统', 2, @aircraft_ac1001_id, 'v3.1', 'v2.0.1', '', 1),
('DEV-1001-03', '数传模块', 3, @aircraft_ac1001_id, 'v1.5', 'v1.0.0', '', 1),
('DEV-1001-04', 'GPS传感器', 4, @aircraft_ac1001_id, 'v2.0', 'v1.1.0', '', 1),
('DEV-1002-01', '任务计算机', 1, @aircraft_ac1002_id, 'v2.0', 'v1.1.0', 'v1.0.3', 1),
('DEV-1002-02', '飞控系统', 2, @aircraft_ac1002_id, 'v3.0', 'v1.9.0', '', 1),
('DEV-1002-03', '图传模块', 3, @aircraft_ac1002_id, 'v1.3', 'v1.0.2', '', 1),
('DEV-1002-04', '激光雷达', 5, @aircraft_ac1002_id, 'v1.0', 'v1.0.0', '', 1),
('DEV-1003-01', '任务计算机', 1, @aircraft_ac1003_id, 'v1.8', 'v1.0.0', 'v0.9.0', 1),
('DEV-1003-02', '飞控系统', 2, @aircraft_ac1003_id, 'v2.5', 'v1.5.0', '', 1),
('DEV-1003-03', '数传模块', 3, @aircraft_ac1003_id, 'v1.2', 'v0.9.0', '', 1),
('DEV-1004-01', '任务计算机', 1, @aircraft_ac1004_id, 'v2.0', 'v1.1.0', 'v1.0.3', 1),
('DEV-1004-02', '图传模块', 3, @aircraft_ac1004_id, 'v1.4', 'v1.2.0', '', 1),
('DEV-1004-03', '相机载荷', 5, @aircraft_ac1004_id, 'v2.0', 'v1.0.0', 'v1.2.0', 1),
('DEV-1005-01', '任务计算机', 1, @aircraft_ac1005_id, 'v3.0', 'v2.0.0', 'v1.5.0', 1),
('DEV-1005-02', '飞控系统', 2, @aircraft_ac1005_id, 'v4.0', 'v2.5.0', '', 1),
('DEV-1005-03', '数传模块', 3, @aircraft_ac1005_id, 'v2.0', 'v1.8.0', '', 1),
('DEV-1005-04', '气象雷达', 5, @aircraft_ac1005_id, 'v1.5', 'v1.2.0', '', 1),
('DEV-1005-05', '惯性导航系统', 4, @aircraft_ac1005_id, 'v2.0', 'v1.5.0', '', 1);

SET @uploader_id = (SELECT user_id FROM users WHERE username = '工程师1' LIMIT 1);

INSERT INTO files (file_code, file_name, file_type, file_size, sha256_hash, storage_path, version, description, uploader_user_id) VALUES
('FILE-FW-001', 'FILE-FW-001_mission_computer_firmware_v1.2.0.bin', 1, 15728640, '5d41402abc4b2a76b9719d911017c5925d41402abc4b2a76b9719d911017c592', 'data/storage/FW/FILE-FW-001_mission_computer_firmware_v1.2.0.bin', 'v1.2.0', '任务计算机固件升级包', @uploader_id),
('FILE-FW-002', 'FILE-FW-002_flight_controller_firmware_v2.0.1.bin', 1, 8388608, '7d793037a0760186574b0282f2f435e77d793037a0760186574b0282f2f435e7', 'data/storage/FW/FILE-FW-002_flight_controller_firmware_v2.0.1.bin', 'v2.0.1', '飞控系统固件升级包', @uploader_id),
('FILE-FW-003', 'FILE-FW-003_data_link_firmware_v1.0.0.bin', 1, 2097152, '098f6bcd4621d373cade4e832627b4f6098f6bcd4621d373cade4e832627b4f6', 'data/storage/FW/FILE-FW-003_data_link_firmware_v1.0.0.bin', 'v1.0.0', '数传模块固件升级包', @uploader_id),
('FILE-SW-001', 'FILE-SW-001_mission_computer_software_v1.0.5.zip', 2, 5242880, '8f14e45fceea167a5a36dedd4bea25438f14e45fceea167a5a36dedd4bea2543', 'data/storage/SW/FILE-SW-001_mission_computer_software_v1.0.5.zip', 'v1.0.5', '任务计算机软件升级包', @uploader_id),
('FILE-FW-004', 'FILE-FW-004_flight_controller_firmware_v2.1.0.bin', 1, 9437184, 'eccbc87e4b5ce2fe28308fd9f2a7baf3eccbc87e4b5ce2fe28308fd9f2a7baf3', 'data/storage/FW/FILE-FW-004_flight_controller_firmware_v2.1.0.bin', 'v2.1.0', '飞控系统最新固件', @uploader_id),
('FILE-FW-005', 'FILE-FW-005_gps_firmware_v1.1.0.bin', 1, 1048576, 'a87ff679a2f3e71d9181a67b7542122ca87ff679a2f3e71d9181a67b7542122c', 'data/storage/FW/FILE-FW-005_gps_firmware_v1.1.0.bin', 'v1.1.0', 'GPS传感器固件升级', @uploader_id),
('FILE-FW-006', 'FILE-FW-006_camera_firmware_v1.2.0.bin', 1, 5242880, 'e4da3b7fbbce2345d7772b0674a318d5e4da3b7fbbce2345d7772b0674a318d5', 'data/storage/FW/FILE-FW-006_camera_firmware_v1.2.0.bin', 'v1.2.0', '相机载荷固件升级', @uploader_id),
('FILE-SW-002', 'FILE-SW-002_lidar_software_v1.0.4.zip', 2, 8388608, '1679091c5a880faf6fb5e6087eb1b2dc1679091c5a880faf6fb5e6087eb1b2dc', 'data/storage/SW/FILE-SW-002_lidar_software_v1.0.4.zip', 'v1.0.4', '激光雷达软件升级', @uploader_id);

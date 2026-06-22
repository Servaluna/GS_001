USE centralserver;

SELECT
    file_id,
    file_code,
    file_name,
    file_type,
    storage_path,
    version,
    status
FROM files
ORDER BY file_id;

UPDATE files
SET file_name = CASE file_code
    WHEN 'FILE-FW-001' THEN 'FILE-FW-001_mission_computer_firmware_v1.2.0.bin'
    WHEN 'FILE-FW-002' THEN 'FILE-FW-002_flight_controller_firmware_v2.0.1.bin'
    WHEN 'FILE-FW-003' THEN 'FILE-FW-003_data_link_firmware_v1.0.0.bin'
    WHEN 'FILE-SW-001' THEN 'FILE-SW-001_mission_computer_software_v1.0.5.zip'
    WHEN 'FILE-FW-004' THEN 'FILE-FW-004_flight_controller_firmware_v2.1.0.bin'
    WHEN 'FILE-FW-005' THEN 'FILE-FW-005_gps_firmware_v1.1.0.bin'
    WHEN 'FILE-FW-006' THEN 'FILE-FW-006_camera_firmware_v1.2.0.bin'
    WHEN 'FILE-SW-002' THEN 'FILE-SW-002_lidar_software_v1.0.4.zip'
    ELSE file_name
END,
storage_path = CASE file_code
    WHEN 'FILE-FW-001' THEN 'data/storage/FW/FILE-FW-001_mission_computer_firmware_v1.2.0.bin'
    WHEN 'FILE-FW-002' THEN 'data/storage/FW/FILE-FW-002_flight_controller_firmware_v2.0.1.bin'
    WHEN 'FILE-FW-003' THEN 'data/storage/FW/FILE-FW-003_data_link_firmware_v1.0.0.bin'
    WHEN 'FILE-SW-001' THEN 'data/storage/SW/FILE-SW-001_mission_computer_software_v1.0.5.zip'
    WHEN 'FILE-FW-004' THEN 'data/storage/FW/FILE-FW-004_flight_controller_firmware_v2.1.0.bin'
    WHEN 'FILE-FW-005' THEN 'data/storage/FW/FILE-FW-005_gps_firmware_v1.1.0.bin'
    WHEN 'FILE-FW-006' THEN 'data/storage/FW/FILE-FW-006_camera_firmware_v1.2.0.bin'
    WHEN 'FILE-SW-002' THEN 'data/storage/SW/FILE-SW-002_lidar_software_v1.0.4.zip'
    ELSE storage_path
END
WHERE file_code IN (
    'FILE-FW-001', 'FILE-FW-002', 'FILE-FW-003', 'FILE-SW-001',
    'FILE-FW-004', 'FILE-FW-005', 'FILE-FW-006', 'FILE-SW-002'
);

SELECT
    file_id,
    file_code,
    file_name,
    file_type,
    storage_path,
    version,
    status
FROM files
ORDER BY file_id;

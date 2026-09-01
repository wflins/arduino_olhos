CREATE TABLE clima_devices (
  id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(100) NOT NULL,
  device_key_hash CHAR(64) NOT NULL UNIQUE,
  device_key_last4 CHAR(4) NOT NULL,
  active TINYINT(1) NOT NULL DEFAULT 1,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_seen_at DATETIME NULL,
  last_ip VARCHAR(45) NULL,
  last_rssi SMALLINT NULL,
  firmware VARCHAR(32) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE clima_readings (
  id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  device_id INT UNSIGNED NOT NULL,
  temperature DECIMAL(5,2) NOT NULL,
  humidity DECIMAL(5,2) NOT NULL,
  rssi SMALLINT NULL,
  firmware VARCHAR(32) NULL,
  measured_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  CONSTRAINT fk_clima_readings_device
    FOREIGN KEY (device_id) REFERENCES clima_devices(id)
    ON DELETE CASCADE,
  INDEX idx_clima_readings_device_date (device_id, measured_at),
  INDEX idx_clima_readings_date (measured_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

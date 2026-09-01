<?php
header('Content-Type: application/json; charset=utf-8');
require __DIR__ . '/../config.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['ok' => false, 'error' => 'method_not_allowed']);
    exit;
}

$body = json_decode(file_get_contents('php://input'), true);
if (!is_array($body)) {
    http_response_code(400);
    echo json_encode(['ok' => false, 'error' => 'invalid_json']);
    exit;
}

$key = trim((string)($body['key'] ?? ''));
$temp = $body['temperature'] ?? null;
$humidity = $body['humidity'] ?? null;
$rssi = isset($body['rssi']) ? (int)$body['rssi'] : null;
$firmware = trim((string)($body['firmware'] ?? ''));

if (!preg_match('/^CB-[A-F0-9]{8}-[A-F0-9]{8}$/', $key) || !is_numeric($temp) || !is_numeric($humidity)) {
    http_response_code(422);
    echo json_encode(['ok' => false, 'error' => 'invalid_payload']);
    exit;
}

$temp = (float)$temp;
$humidity = (float)$humidity;
if ($temp < -50 || $temp > 100 || $humidity < 0 || $humidity > 100) {
    http_response_code(422);
    echo json_encode(['ok' => false, 'error' => 'out_of_range']);
    exit;
}

$hash = hash('sha256', $key);
$pdo = db();
$stmt = $pdo->prepare('SELECT id FROM clima_devices WHERE device_key_hash = ? AND active = 1 LIMIT 1');
$stmt->execute([$hash]);
$device = $stmt->fetch();

if (!$device) {
    http_response_code(403);
    echo json_encode(['ok' => false, 'error' => 'device_not_registered']);
    exit;
}

$pdo->beginTransaction();
try {
    $stmt = $pdo->prepare('INSERT INTO clima_readings (device_id, temperature, humidity, rssi, firmware) VALUES (?, ?, ?, ?, ?)');
    $stmt->execute([$device['id'], $temp, $humidity, $rssi, $firmware ?: null]);

    $stmt = $pdo->prepare('UPDATE clima_devices SET last_seen_at = NOW(), last_ip = ?, last_rssi = ?, firmware = ? WHERE id = ?');
    $stmt->execute([$_SERVER['REMOTE_ADDR'] ?? null, $rssi, $firmware ?: null, $device['id']]);
    $pdo->commit();
} catch (Throwable $e) {
    $pdo->rollBack();
    http_response_code(500);
    echo json_encode(['ok' => false, 'error' => 'database_error']);
    exit;
}

echo json_encode(['ok' => true]);

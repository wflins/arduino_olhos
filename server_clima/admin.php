<?php
session_start();
require __DIR__ . '/config.php';

$error = '';
$message = '';

if (isset($_POST['logout'])) {
    session_destroy();
    header('Location: admin.php');
    exit;
}

if (!($_SESSION['clima_admin'] ?? false)) {
    if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['password'])) {
        if (password_verify((string)$_POST['password'], ADMIN_PASSWORD_HASH)) {
            $_SESSION['clima_admin'] = true;
            header('Location: admin.php');
            exit;
        }
        $error = 'Senha invalida.';
    }
    ?>
    <!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>ClimaBox Admin</title>
    <style>body{font-family:Arial;max-width:520px;margin:40px auto;padding:18px;background:#f4f6f8}.card{background:#fff;padding:22px;border-radius:12px}input,button{width:100%;padding:12px;margin:8px 0;box-sizing:border-box}</style></head><body><div class="card">
    <h2>ClimaBox</h2><p>Administracao de dispositivos</p><?php if($error): ?><p><?=htmlspecialchars($error)?></p><?php endif; ?>
    <form method="post"><input type="password" name="password" placeholder="Senha administrativa" required><button>Entrar</button></form>
    </div></body></html><?php
    exit;
}

$pdo = db();

if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['device_key'], $_POST['name'])) {
    $key = strtoupper(trim((string)$_POST['device_key']));
    $name = trim((string)$_POST['name']);
    if (!preg_match('/^CB-[A-F0-9]{8}-[A-F0-9]{8}$/', $key)) {
        $error = 'Chave invalida. Use o formato CB-XXXXXXXX-XXXXXXXX.';
    } elseif ($name === '') {
        $error = 'Informe um nome para o dispositivo.';
    } else {
        try {
            $stmt = $pdo->prepare('INSERT INTO clima_devices (name, device_key_hash, device_key_last4) VALUES (?, ?, ?)');
            $stmt->execute([$name, hash('sha256', $key), substr($key, -4)]);
            $message = 'Dispositivo cadastrado com sucesso.';
        } catch (PDOException $e) {
            $error = $e->getCode() === '23000' ? 'Essa chave ja esta cadastrada.' : 'Erro ao cadastrar dispositivo.';
        }
    }
}

$devices = $pdo->query('SELECT id,name,active,device_key_last4,created_at,last_seen_at,last_rssi,firmware FROM clima_devices ORDER BY id DESC')->fetchAll();
?>
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>ClimaBox Admin</title>
<style>body{font-family:Arial;max-width:900px;margin:30px auto;padding:16px;background:#f4f6f8}.card{background:#fff;padding:20px;border-radius:12px;margin-bottom:18px}input,button{padding:10px;margin:5px 0;box-sizing:border-box}input{width:100%}button{cursor:pointer}table{width:100%;border-collapse:collapse;font-size:14px}th,td{text-align:left;padding:9px;border-bottom:1px solid #ddd}.ok{color:#087f23}.err{color:#b00020}</style></head><body>
<div class="card"><div style="float:right"><form method="post"><button name="logout" value="1">Sair</button></form></div><h2>ClimaBox</h2><p>Cadastro de dispositivos</p>
<?php if($message): ?><p class="ok"><?=htmlspecialchars($message)?></p><?php endif; ?><?php if($error): ?><p class="err"><?=htmlspecialchars($error)?></p><?php endif; ?>
<form method="post"><label>Nome</label><input name="name" placeholder="Ex.: ClimaBox Sala" required><label>Chave mostrada pelo ClimaBox</label><input name="device_key" placeholder="CB-XXXXXXXX-XXXXXXXX" maxlength="20" required><button>Cadastrar dispositivo</button></form></div>
<div class="card"><h3>Dispositivos</h3><table><tr><th>Nome</th><th>Chave</th><th>Ultima leitura</th><th>RSSI</th><th>Firmware</th></tr>
<?php foreach($devices as $d): ?><tr><td><?=htmlspecialchars($d['name'])?></td><td>...<?=htmlspecialchars($d['device_key_last4'])?></td><td><?=htmlspecialchars($d['last_seen_at'] ?: 'Nunca')?></td><td><?=htmlspecialchars($d['last_rssi'] ?? '-')?></td><td><?=htmlspecialchars($d['firmware'] ?: '-')?></td></tr><?php endforeach; ?>
</table></div></body></html>

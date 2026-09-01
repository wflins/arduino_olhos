<?php
// Copie para config.php no servidor e preencha fora do Git.

const DB_DSN = 'mysql:host=localhost;dbname=SEU_BANCO;charset=utf8mb4';
const DB_USER = 'SEU_USUARIO';
const DB_PASS = 'SUA_SENHA';

// Gere com: password_hash('SUA_SENHA_ADMIN', PASSWORD_DEFAULT)
const ADMIN_PASSWORD_HASH = '$2y$10$SUBSTITUA_POR_UM_HASH_REAL';

function db(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO(DB_DSN, DB_USER, DB_PASS, [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            PDO::ATTR_EMULATE_PREPARES => false,
        ]);
    }
    return $pdo;
}

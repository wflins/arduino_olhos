/*
  ClimaBox - NodeMCU ESP8266 + OLED 0,91" + DHT11 + configuracao WiFi

  Ligacoes:

  OLED SSD1306 0,91" I2C (128x32)
    VCC -> 3V3
    GND -> GND
    SCL/SCK -> D1 (GPIO5)
    SDA -> D2 (GPIO4)

  DHT11
    VCC  -> 3V3
    GND  -> GND
    DATA -> D5 (GPIO14)

  Bibliotecas necessarias:
    - Adafruit GFX Library
    - Adafruit SSD1306
    - DHT sensor library by Adafruit
    - Adafruit Unified Sensor

  WiFi:
    1. Ao iniciar, tenta conectar usando as credenciais salvas pelo ESP8266.
    2. Se nao conectar em 15 segundos, cria um AP temporario:
         ClimaBox-XXXX
    3. Conecte-se ao AP e abra http://192.168.4.1
       (ha tambem um captive portal via DNS).
    4. Escolha/informe a rede e a senha.
    5. O ESP salva as credenciais e reinicia.

  Monitor Serial: 115200 baud
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#define I2C_SDA D2
#define I2C_SCL D1

#define DHT_PIN D5
#define DHT_TYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define TEMPO_CONEXAO_WIFI 15000UL
#define INTERVALO_LEITURA 2500UL

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server(80);
DNSServer dnsServer;

bool oledOK = false;
bool modoConfiguracao = false;
unsigned long ultimaLeitura = 0;
String apSSID;

void limparOLED() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
}

void mostrarMensagem(const String &l1, const String &l2 = "", const String &l3 = "") {
  if (!oledOK) return;

  limparOLED();
  display.setCursor(0, 0);
  display.println(l1);
  display.setCursor(0, 11);
  display.println(l2);
  display.setCursor(0, 22);
  display.println(l3);
  display.display();
}

String htmlEscape(String texto) {
  texto.replace("&", "&amp;");
  texto.replace("<", "&lt;");
  texto.replace(">", "&gt;");
  texto.replace("\"", "&quot;");
  texto.replace("'", "&#39;");
  return texto;
}

String paginaConfiguracao(const String &mensagem = "") {
  String html;
  html.reserve(6000);

  html += F("<!doctype html><html lang='pt-br'><head>");
  html += F("<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>ClimaBox - WiFi</title><style>");
  html += F("body{font-family:Arial,sans-serif;background:#f3f5f7;margin:0;padding:20px;color:#222}");
  html += F(".box{max-width:460px;margin:20px auto;background:#fff;padding:22px;border-radius:16px;box-shadow:0 4px 18px #0002}");
  html += F("h1{font-size:24px;margin:0 0 6px}.sub{color:#666;margin-bottom:20px}");
  html += F("label{display:block;font-weight:bold;margin:14px 0 6px}select,input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #bbb;border-radius:9px;font-size:16px}");
  html += F("button{width:100%;margin-top:18px;padding:13px;border:0;border-radius:9px;background:#222;color:#fff;font-size:16px;font-weight:bold}");
  html += F(".msg{background:#eef7ee;padding:10px;border-radius:8px;margin-bottom:15px}.small{font-size:13px;color:#777;margin-top:16px}");
  html += F("</style></head><body><div class='box'><h1>ClimaBox</h1><div class='sub'>Configuracao de Wi-Fi</div>");

  if (mensagem.length()) {
    html += F("<div class='msg'>");
    html += htmlEscape(mensagem);
    html += F("</div>");
  }

  int n = WiFi.scanNetworks();

  html += F("<form method='POST' action='/salvar'>");
  html += F("<label>Rede Wi-Fi</label><select name='ssid' required>");

  if (n <= 0) {
    html += F("<option value=''>Nenhuma rede encontrada</option>");
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      bool aberta = (WiFi.encryptionType(i) == ENC_TYPE_NONE);

      html += F("<option value=\"");
      html += htmlEscape(ssid);
      html += F("\">");
      html += htmlEscape(ssid);
      html += F(" (");
      html += String(rssi);
      html += F(" dBm");
      if (aberta) html += F(", aberta");
      html += F(")</option>");
    }
  }

  html += F("</select>");
  html += F("<label>Ou digite o SSID</label><input name='ssid_manual' maxlength='32' placeholder='Opcional'>");
  html += F("<label>Senha</label><input name='senha' type='password' maxlength='64' placeholder='Senha da rede'>");
  html += F("<button type='submit'>Salvar e conectar</button></form>");
  html += F("<div class='small'>Se a pagina nao abrir automaticamente, acesse <b>192.168.4.1</b>.</div>");
  html += F("</div></body></html>");

  WiFi.scanDelete();
  return html;
}

void responderPortal() {
  server.send(200, "text/html; charset=utf-8", paginaConfiguracao());
}

void salvarWiFi() {
  String ssid = server.arg("ssid_manual");
  ssid.trim();
  if (!ssid.length()) {
    ssid = server.arg("ssid");
  }

  String senha = server.arg("senha");

  if (!ssid.length()) {
    server.send(400, "text/html; charset=utf-8", paginaConfiguracao("Informe uma rede Wi-Fi."));
    return;
  }

  Serial.print(F("Nova rede informada: "));
  Serial.println(ssid);

  // O core ESP8266 grava as credenciais na flash quando persistent(true).
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), senha.c_str());

  mostrarMensagem("WiFi configurado", ssid, "Reiniciando...");

  String html = F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:Arial;padding:30px'><h2>ClimaBox</h2><p>Wi-Fi salvo.</p><p>O dispositivo sera reiniciado e tentara conectar a <b>");
  html += htmlEscape(ssid);
  html += F("</b>.</p></body></html>");

  server.send(200, "text/html; charset=utf-8", html);
  delay(1500);
  ESP.restart();
}

void iniciarModoConfiguracao() {
  modoConfiguracao = true;

  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);

  uint32_t chipId = ESP.getChipId();
  char sufixo[5];
  snprintf(sufixo, sizeof(sufixo), "%04X", (unsigned int)(chipId & 0xFFFF));
  apSSID = "ClimaBox-" + String(sufixo);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netmask(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netmask);
  WiFi.softAP(apSSID.c_str());

  dnsServer.start(53, "*", apIP);

  server.on("/", HTTP_GET, responderPortal);
  server.on("/salvar", HTTP_POST, salvarWiFi);

  // Endpoints comuns usados por Android/iOS/Windows para detectar captive portal.
  server.on("/generate_204", HTTP_ANY, responderPortal);
  server.on("/gen_204", HTTP_ANY, responderPortal);
  server.on("/hotspot-detect.html", HTTP_ANY, responderPortal);
  server.on("/connecttest.txt", HTTP_ANY, responderPortal);
  server.on("/ncsi.txt", HTTP_ANY, responderPortal);
  server.onNotFound(responderPortal);
  server.begin();

  Serial.println();
  Serial.println(F("=== MODO CONFIGURACAO WIFI ==="));
  Serial.print(F("Rede temporaria: "));
  Serial.println(apSSID);
  Serial.println(F("Acesse: http://192.168.4.1"));
  Serial.println(F("==============================="));

  mostrarMensagem("Configurar WiFi", apSSID, "192.168.4.1");
}

bool conectarWiFiSalvo() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();

  Serial.print(F("Tentando WiFi salvo"));
  mostrarMensagem("ClimaBox", "Conectando WiFi...", "");

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < TEMPO_CONEXAO_WIFI) {
    Serial.print('.');
    delay(400);
    yield();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi conectado."));
    Serial.print(F("SSID: "));
    Serial.println(WiFi.SSID());
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    mostrarMensagem("WiFi conectado", WiFi.SSID(), WiFi.localIP().toString());
    delay(1800);
    return true;
  }

  Serial.println(F("Nao foi possivel conectar ao WiFi salvo."));
  return false;
}

void mostrarErroDHT() {
  mostrarMensagem("ClimaBox", "Erro no DHT11", WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi offline");
}

void mostrarLeitura(float temperatura, float umidade) {
  if (!oledOK) return;

  limparOLED();

  // Cabecalho + indicador simples de WiFi.
  display.setCursor(0, 0);
  display.print(F("ClimaBox"));
  display.setCursor(112, 0);
  display.print(WiFi.status() == WL_CONNECTED ? F("Wi") : F("--"));

  display.setCursor(0, 12);
  display.print(F("Temp: "));
  display.print(temperatura, 1);
  display.print(F(" C"));

  display.setCursor(0, 23);
  display.print(F("Umid: "));
  display.print(umidade, 0);
  display.print(F(" %"));

  display.display();
}

void lerDHT11() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  Serial.println(F("--------------------------------"));
  Serial.println(F("ClimaBox - leitura local"));

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println(F("DHT11: ERRO na leitura"));
    mostrarErroDHT();
  } else {
    Serial.print(F("Temperatura: "));
    Serial.print(temperatura, 1);
    Serial.println(F(" C"));

    Serial.print(F("Umidade: "));
    Serial.print(umidade, 0);
    Serial.println(F(" %"));

    Serial.print(F("WiFi: "));
    Serial.println(WiFi.status() == WL_CONNECTED ? WiFi.SSID() : F("offline"));

    mostrarLeitura(temperatura, umidade);
  }

  Serial.println(F("--------------------------------"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F(" ClimaBox - OLED + DHT11 + WiFi"));
  Serial.println(F("================================"));

  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (!oledOK) {
    Serial.println(F("OLED: ERRO / nao encontrado em 0x3C"));
  } else {
    Serial.println(F("OLED: OK (0x3C)"));
    mostrarMensagem("ClimaBox", "Iniciando...", "");
  }

  dht.begin();
  delay(2200);

  if (!conectarWiFiSalvo()) {
    iniciarModoConfiguracao();
  } else {
    lerDHT11();
    ultimaLeitura = millis();
  }
}

void loop() {
  if (modoConfiguracao) {
    dnsServer.processNextRequest();
    server.handleClient();
    yield();
    return;
  }

  // Se perder WiFi durante o uso, o ESP8266 tenta reconectar automaticamente.
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = millis();
    lerDHT11();
  }

  yield();
}

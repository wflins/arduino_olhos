/*
  ClimaBox - ESP8266 + OLED 0,91" + DHT11 + WiFi + API

  OLED SSD1306 128x32 I2C
    VCC -> 3V3
    GND -> GND
    SCL/SCK -> D1 (GPIO5)
    SDA -> D2 (GPIO4)

  DHT11
    VCC -> 3V3
    GND -> GND
    DATA -> D5 (GPIO14)

  Bibliotecas adicionais: nenhuma alem das ja usadas.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>

#define I2C_SDA D2
#define I2C_SCL D1
#define DHT_PIN D5
#define DHT_TYPE DHT11
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define EEPROM_SIZE 64
#define KEY_MAGIC 0x43
#define FW_VERSION "0.4"

const char* API_URL = "https://clima.lins.dev.br/api/measure.php";
const unsigned long INTERVALO_LEITURA = 2500;
const unsigned long INTERVALO_ENVIO = 300000; // 5 minutos
const unsigned long WIFI_TIMEOUT = 15000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server(80);
DNSServer dnsServer;

bool oledOK = false;
bool modoConfig = false;
bool dispositivoRegistrado = true;
String deviceKey;
String apName;
unsigned long ultimaLeitura = 0;
unsigned long ultimoEnvio = 0;
float ultimaTemp = NAN;
float ultimaUmid = NAN;

void oled3(const String& a, const String& b = "", const String& c = "") {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println(a);
  display.setCursor(0, 11); display.println(b);
  display.setCursor(0, 22); display.println(c);
  display.display();
}

String hex8(uint32_t v) {
  char buf[9];
  snprintf(buf, sizeof(buf), "%08X", v);
  return String(buf);
}

void gerarOuCarregarChave() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) == KEY_MAGIC) {
    char buf[20];
    for (int i = 0; i < 19; i++) buf[i] = char(EEPROM.read(i + 1));
    buf[19] = 0;
    deviceKey = String(buf);
    if (!deviceKey.startsWith("CB-")) deviceKey = "";
  }

  if (deviceKey.length() == 0) {
    uint32_t a = ESP.getChipId() ^ ESP.getFlashChipId() ^ micros();
    delay(5);
    uint32_t b = ESP.getCycleCount() ^ micros() ^ (ESP.getChipId() << 7);
    deviceKey = "CB-" + hex8(a) + "-" + hex8(b);

    EEPROM.write(0, KEY_MAGIC);
    for (int i = 0; i < 19; i++) EEPROM.write(i + 1, deviceKey[i]);
    EEPROM.commit();
  }

  Serial.print(F("Chave do dispositivo: "));
  Serial.println(deviceKey);
}

String htmlConfig() {
  int n = WiFi.scanNetworks();
  String h = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<style>body{font-family:Arial;max-width:480px;margin:30px auto;padding:16px}input,select,button{width:100%;padding:12px;margin:7px 0;box-sizing:border-box}</style></head><body>";
  h += "<h2>ClimaBox</h2><p>Configure a rede Wi-Fi.</p><form method='post' action='/save'><select name='ssid'>";
  for (int i = 0; i < n; i++) {
    h += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  h += "</select><input name='pass' type='password' placeholder='Senha'><button>Salvar e conectar</button></form>";
  h += "<hr><small>Dispositivo: " + deviceKey + "</small></body></html>";
  return h;
}

void iniciarPortal() {
  modoConfig = true;
  WiFi.mode(WIFI_AP_STA);
  apName = "ClimaBox-" + hex8(ESP.getChipId()).substring(4);
  WiFi.softAP(apName.c_str());
  IPAddress ip = WiFi.softAPIP();
  dnsServer.start(53, "*", ip);

  server.on("/", [](){ server.send(200, "text/html", htmlConfig()); });
  server.on("/generate_204", [](){ server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.on("/hotspot-detect.html", [](){ server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.on("/save", HTTP_POST, [](){
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() == 0) { server.send(400, "text/plain", "SSID obrigatorio"); return; }
    server.send(200, "text/html", "<h2>ClimaBox</h2><p>Rede salva. Reiniciando...</p>");
    delay(500);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    delay(1000);
    ESP.restart();
  });
  server.onNotFound([](){ server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.begin();

  oled3("Configurar WiFi", apName, "192.168.4.1");
  Serial.println("AP: " + apName);
  Serial.println(F("Abra: http://192.168.4.1"));
}

bool conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  oled3("ClimaBox", "Conectando WiFi...", "");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_TIMEOUT) {
    delay(300);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi OK: ")); Serial.println(WiFi.SSID());
    Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
    oled3("WiFi conectado", WiFi.SSID(), WiFi.localIP().toString());
    delay(1500);
    return true;
  }
  return false;
}

void mostrarLeitura() {
  if (isnan(ultimaTemp) || isnan(ultimaUmid)) {
    oled3("ClimaBox", "Erro no DHT11", "");
    return;
  }
  String status = WiFi.status() == WL_CONNECTED ? "WiFi OK" : "Sem WiFi";
  if (!dispositivoRegistrado) status = "CADASTRAR CHAVE";
  oled3("T " + String(ultimaTemp,1) + "C  U " + String(ultimaUmid,0) + "%", status, dispositivoRegistrado ? "" : deviceKey.substring(0,19));
}

void lerDHT() {
  ultimaUmid = dht.readHumidity();
  ultimaTemp = dht.readTemperature();
  if (isnan(ultimaTemp) || isnan(ultimaUmid)) {
    Serial.println(F("DHT11: erro"));
  } else {
    Serial.printf("Temperatura: %.1f C | Umidade: %.0f %%\n", ultimaTemp, ultimaUmid);
  }
  mostrarLeitura();
}

void enviarMedicao() {
  if (WiFi.status() != WL_CONNECTED || isnan(ultimaTemp) || isnan(ultimaUmid)) return;

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  // Prototipo: HTTPS criptografado, mas sem validacao do certificado.
  // Na versao final devemos fixar CA/certificado.
  client->setInsecure();

  HTTPClient https;
  if (!https.begin(*client, API_URL)) return;
  https.addHeader("Content-Type", "application/json");

  String json = "{\"key\":\"" + deviceKey + "\",\"temperature\":" + String(ultimaTemp,1) +
                ",\"humidity\":" + String(ultimaUmid,0) + ",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"firmware\":\"" FW_VERSION "\"}";

  int code = https.POST(json);
  String resposta = https.getString();
  Serial.printf("API HTTP %d: %s\n", code, resposta.c_str());

  dispositivoRegistrado = (code != 403);
  if (code == 403) {
    oled3("Cadastre no site", deviceKey.substring(0,11), deviceKey.substring(11));
    delay(5000);
  }
  https.end();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin(I2C_SDA, I2C_SCL);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  dht.begin();

  gerarOuCarregarChave();
  if (oledOK) {
    oled3("ClimaBox " FW_VERSION, "Chave:", deviceKey.substring(0,19));
    delay(1800);
  }

  if (!conectarWifi()) {
    iniciarPortal();
    return;
  }

  delay(2200);
  lerDHT();
  enviarMedicao();
  ultimaLeitura = millis();
  ultimoEnvio = millis();
}

void loop() {
  if (modoConfig) {
    dnsServer.processNextRequest();
    server.handleClient();
    yield();
    return;
  }

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = millis();
    lerDHT();
  }

  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    if (WiFi.status() != WL_CONNECTED) conectarWifi();
    enviarMedicao();
  }
  yield();
}

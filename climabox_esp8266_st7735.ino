#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <qrcode.h>

// ============================================================
// ClimaBox - NodeMCU ESP8266 + TFT ST7735 1.8" 128x160
// ============================================================
//
// TFT -> NodeMCU ESP8266
// LED   -> 3V3
// SCK   -> D5
// SDA   -> D7 (MOSI)
// A0/DC -> D2
// RESET -> D1
// CS    -> D8
// GND   -> G
// VCC   -> VU/5V (para o modulo usado neste projeto)
//
// Botao FLASH do NodeMCU = GPIO0. Segure por 5 segundos, com o
// aparelho ja ligado, para reabrir o portal de configuracao.
// ============================================================

#define TFT_CS   D8
#define TFT_RST  D1
#define TFT_DC   D2
#define CONFIG_BUTTON 0

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox"; // minimo 8 caracteres
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;

String cidade = "Manaus";
String uf = "AM";
float latitude = -3.1190f;
float longitude = -60.0217f;
bool coordenadasValidas = true;

float temperatura = NAN;
float sensacao = NAN;
float umidade = NAN;
float vento = NAN;
int weatherCode = -1;
bool isDay = true;
String weatherTime = "";

unsigned long ultimaConsulta = 0;

// ============================================================
// Utilitarios da tela
// ============================================================

void telaLimpa(uint16_t cor = ST77XX_BLACK) {
  tft.fillScreen(cor);
}

void cabecalho(const String& texto, uint16_t cor = ST77XX_CYAN) {
  telaLimpa();
  tft.setTextWrap(false);
  tft.setTextColor(cor);
  tft.setTextSize(2);
  tft.setCursor(6, 5);
  tft.print(texto);
  tft.drawFastHLine(0, 25, 160, tft.color565(70, 70, 70));
}

void textoLinha(int y, const String& texto, uint16_t cor = ST77XX_WHITE, uint8_t tamanho = 1) {
  tft.setTextColor(cor);
  tft.setTextSize(tamanho);
  tft.setCursor(6, y);
  tft.print(texto);
}

void telaErro(const String& titulo, const String& detalhe) {
  cabecalho(titulo, ST77XX_RED);
  textoLinha(38, detalhe, ST77XX_WHITE, 1);
}

// ============================================================
// Persistencia da cidade/coordenadas
// ============================================================

bool salvarConfig() {
  JsonDocument doc;
  doc["cidade"] = cidade;
  doc["uf"] = uf;
  doc["latitude"] = latitude;
  doc["longitude"] = longitude;
  doc["coords_ok"] = coordenadasValidas;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial.println("Falha ao abrir arquivo de configuracao para escrita");
    return false;
  }

  serializeJson(doc, f);
  f.close();
  return true;
}

void carregarConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) return;

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, f);
  f.close();

  if (erro) {
    Serial.print("Erro ao ler configuracao: ");
    Serial.println(erro.c_str());
    return;
  }

  cidade = doc["cidade"] | "Manaus";
  uf = doc["uf"] | "AM";
  latitude = doc["latitude"] | -3.1190f;
  longitude = doc["longitude"] | -60.0217f;
  coordenadasValidas = doc["coords_ok"] | true;
}

// ============================================================
// URL encode
// ============================================================

String urlEncode(const String& value) {
  String encoded;
  char hex[4];

  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      encoded += hex;
    }
  }
  return encoded;
}

// ============================================================
// QR Code para conectar no AP de configuracao
// ============================================================

void desenharQrCode(const String& conteudo, int origemX, int origemY, int escala) {
  const uint8_t versao = 3;
  uint16_t tamanhoBuffer = qrcode_getBufferSize(versao);
  uint8_t* dados = (uint8_t*)malloc(tamanhoBuffer);

  if (!dados) {
    textoLinha(50, "Sem memoria para QR", ST77XX_RED, 1);
    return;
  }

  QRCode qr;
  int8_t status = qrcode_initText(&qr, dados, versao, ECC_LOW, conteudo.c_str());
  if (status < 0) {
    free(dados);
    textoLinha(50, "Erro ao gerar QR", ST77XX_RED, 1);
    return;
  }

  const int quiet = 2;
  int total = (qr.size + quiet * 2) * escala;
  tft.fillRect(origemX, origemY, total, total, ST77XX_WHITE);

  for (uint8_t y = 0; y < qr.size; y++) {
    for (uint8_t x = 0; x < qr.size; x++) {
      if (qrcode_getModule(&qr, x, y)) {
        int px = origemX + (x + quiet) * escala;
        int py = origemY + (y + quiet) * escala;
        tft.fillRect(px, py, escala, escala, ST77XX_BLACK);
      }
    }
  }

  free(dados);
}

void mostrarPortalNaTela() {
  telaLimpa(ST77XX_BLACK);

  // QR Wi-Fi: a camera do celular oferece conexao direta ao AP.
  String payload = String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASSWORD + ";;";
  desenharQrCode(payload, 4, 4, 3);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(112, 8);
  tft.print("CONFIG");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(112, 25);
  tft.print("Leia o");
  tft.setCursor(112, 35);
  tft.print("QR com");
  tft.setCursor(112, 45);
  tft.print("celular");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(112, 65);
  tft.print("Depois:");

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(112, 78);
  tft.print("192.168.");
  tft.setCursor(112, 88);
  tft.print("4.1");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 116);
  tft.print("AP: ClimaBox-Setup  senha: climabox");
}

void callbackAP(WiFiManager* wm) {
  Serial.println("Portal de configuracao iniciado");
  Serial.print("IP do portal: ");
  Serial.println(WiFi.softAPIP());
  mostrarPortalNaTela();
}

// ============================================================
// Geocodificacao Open-Meteo
// ============================================================

bool geocodificarCidade() {
  if (WiFi.status() != WL_CONNECTED) return false;

  cabecalho("LOCAL");
  textoLinha(40, "Localizando cidade...");
  textoLinha(58, cidade + " - " + uf, ST77XX_YELLOW, 1);

  String busca = cidade;
  if (uf.length() > 0) busca += ", " + uf;

  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
               urlEncode(busca) +
               "&count=1&language=pt&format=json&countryCode=BR";

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) {
    Serial.println("Falha em http.begin geocoding");
    return false;
  }

  http.setTimeout(10000);
  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Geocoding HTTP: ");
    Serial.println(codigo);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.print("Geocoding JSON: ");
    Serial.println(erro.c_str());
    return false;
  }

  JsonArray resultados = doc["results"].as<JsonArray>();
  if (resultados.isNull() || resultados.size() == 0) {
    Serial.println("Cidade nao encontrada");
    return false;
  }

  JsonObject local = resultados[0];
  latitude = local["latitude"].as<float>();
  longitude = local["longitude"].as<float>();

  const char* nomeEncontrado = local["name"] | nullptr;
  if (nomeEncontrado && strlen(nomeEncontrado) > 0) cidade = nomeEncontrado;

  const char* admin1 = local["admin1"] | nullptr;
  Serial.print("Local encontrado: ");
  Serial.print(cidade);
  if (admin1) {
    Serial.print(" / ");
    Serial.print(admin1);
  }
  Serial.print(" -> ");
  Serial.print(latitude, 5);
  Serial.print(", ");
  Serial.println(longitude, 5);

  coordenadasValidas = true;
  salvarConfig();
  return true;
}

// ============================================================
// Portal Wi-Fi + cidade/UF
// ============================================================

bool abrirPortalConfiguracao() {
  WiFiManager wm;
  wm.setAPCallback(callbackAP);
  wm.setConfigPortalTimeout(0); // permanece aberto ate concluir
  wm.setMinimumSignalQuality(8);
  wm.setRemoveDuplicateAPs(true);
  wm.setClass("invert");

  char cidadeBuffer[41];
  char ufBuffer[5];
  memset(cidadeBuffer, 0, sizeof(cidadeBuffer));
  memset(ufBuffer, 0, sizeof(ufBuffer));
  cidade.toCharArray(cidadeBuffer, sizeof(cidadeBuffer));
  uf.toCharArray(ufBuffer, sizeof(ufBuffer));

  WiFiManagerParameter tituloLocal("<p><strong>Local do clima</strong></p>");
  WiFiManagerParameter campoCidade("cidade", "Cidade", cidadeBuffer, 40);
  WiFiManagerParameter campoUF("uf", "UF (ex.: AM)", ufBuffer, 4);

  wm.addParameter(&tituloLocal);
  wm.addParameter(&campoCidade);
  wm.addParameter(&campoUF);

  mostrarPortalNaTela();

  bool ok = wm.startConfigPortal(AP_SSID, AP_PASSWORD);
  if (!ok || WiFi.status() != WL_CONNECTED) {
    Serial.println("Portal encerrado sem conexao");
    return false;
  }

  String novaCidade = String(campoCidade.getValue());
  String novaUf = String(campoUF.getValue());
  novaCidade.trim();
  novaUf.trim();
  novaUf.toUpperCase();

  if (novaCidade.length() > 0) cidade = novaCidade;
  if (novaUf.length() > 0) uf = novaUf;

  coordenadasValidas = false;
  salvarConfig();

  cabecalho("WIFI OK", ST77XX_GREEN);
  textoLinha(42, WiFi.SSID(), ST77XX_WHITE, 1);
  textoLinha(62, cidade + " - " + uf, ST77XX_YELLOW, 1);
  delay(1200);

  if (!geocodificarCidade()) {
    coordenadasValidas = false;
    salvarConfig();
    telaErro("CIDADE", "Nao encontrada. Reconfigure.");
    delay(3000);
    return false;
  }

  return true;
}

// ============================================================
// Conexao automatica
// ============================================================

bool haWifiSalvo() {
  String ssidSalvo = WiFi.SSID();
  return ssidSalvo.length() > 0;
}

bool conectarWifiSalvo() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  if (!haWifiSalvo()) {
    Serial.println("Nenhum Wi-Fi salvo");
    return false;
  }

  cabecalho("CLIMABOX");
  textoLinha(38, "Conectando Wi-Fi...");
  textoLinha(55, WiFi.SSID(), ST77XX_YELLOW, 1);

  WiFi.begin();
  unsigned long inicio = millis();
  int ultimoSegundo = -1;

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_CONNECT_TIMEOUT) {
    int restante = (int)((WIFI_CONNECT_TIMEOUT - (millis() - inicio) + 999) / 1000);
    if (restante != ultimoSegundo) {
      ultimoSegundo = restante;
      tft.fillRect(6, 78, 150, 22, ST77XX_BLACK);
      textoLinha(80, "Tentativa: " + String(restante) + "s", ST77XX_WHITE, 1);
    }
    delay(100);
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Conectado em ");
    Serial.print(WiFi.SSID());
    Serial.print(" IP ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Nao foi possivel conectar ao Wi-Fi salvo");
  return false;
}

// ============================================================
// Clima Open-Meteo
// ============================================================

String descricaoTempo(int codigo) {
  if (codigo == 0) return "Ceu limpo";
  if (codigo == 1) return "Quase limpo";
  if (codigo == 2) return "Parc. nublado";
  if (codigo == 3) return "Nublado";
  if (codigo == 45 || codigo == 48) return "Neblina";
  if (codigo >= 51 && codigo <= 57) return "Garoa";
  if (codigo >= 61 && codigo <= 67) return "Chuva";
  if (codigo >= 71 && codigo <= 77) return "Neve";
  if (codigo >= 80 && codigo <= 82) return "Pancadas";
  if (codigo >= 85 && codigo <= 86) return "Neve forte";
  if (codigo >= 95 && codigo <= 99) return "Trovoadas";
  return "Tempo variavel";
}

void desenharSol(int cx, int cy) {
  uint16_t amarelo = ST77XX_YELLOW;
  tft.fillCircle(cx, cy, 11, amarelo);
  for (int a = 0; a < 8; a++) {
    float ang = a * PI / 4.0f;
    int x1 = cx + cos(ang) * 15;
    int y1 = cy + sin(ang) * 15;
    int x2 = cx + cos(ang) * 20;
    int y2 = cy + sin(ang) * 20;
    tft.drawLine(x1, y1, x2, y2, amarelo);
  }
}

void desenharLua(int cx, int cy) {
  tft.fillCircle(cx, cy, 13, ST77XX_WHITE);
  tft.fillCircle(cx + 6, cy - 4, 12, ST77XX_BLACK);
}

void desenharNuvem(int x, int y, uint16_t cor) {
  tft.fillCircle(x + 11, y + 12, 9, cor);
  tft.fillCircle(x + 23, y + 8, 12, cor);
  tft.fillCircle(x + 36, y + 13, 9, cor);
  tft.fillRoundRect(x + 7, y + 12, 34, 12, 5, cor);
}

void desenharChuva(int x, int y) {
  uint16_t cinza = tft.color565(190, 200, 210);
  uint16_t azul = tft.color565(80, 170, 255);
  desenharNuvem(x, y, cinza);
  for (int i = 0; i < 4; i++) {
    int xx = x + 10 + i * 8;
    tft.drawLine(xx, y + 29, xx - 3, y + 37, azul);
  }
}

void desenharIconeClima(int code, bool dia) {
  int x = 10;
  int y = 40;

  if (!dia && code <= 3) {
    desenharLua(32, 58);
    if (code >= 2) desenharNuvem(15, 54, tft.color565(190, 200, 210));
    return;
  }

  if (code == 0) {
    desenharSol(32, 58);
  } else if (code == 1 || code == 2) {
    desenharSol(25, 51);
    desenharNuvem(17, 52, tft.color565(210, 215, 220));
  } else if (code == 3 || code == 45 || code == 48) {
    desenharNuvem(x, y + 8, tft.color565(190, 200, 210));
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) || (code >= 95 && code <= 99)) {
    desenharChuva(x, y + 3);
  } else {
    desenharNuvem(x, y + 8, tft.color565(190, 200, 210));
  }
}

String horaDaConsulta() {
  if (weatherTime.length() >= 16) return weatherTime.substring(11, 16);
  return "--:--";
}

void mostrarClima() {
  telaLimpa();
  tft.setTextWrap(false);

  String nome = cidade;
  if (uf.length()) nome += " - " + uf;
  if (nome.length() > 20) nome = nome.substring(0, 20);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print(nome);

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(124, 5);
  tft.print("WiFi");

  tft.drawFastHLine(0, 17, 160, tft.color565(70, 70, 70));

  desenharIconeClima(weatherCode, isDay);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(68, 32);
  if (!isnan(temperatura)) {
    tft.print(temperatura, 0);
    tft.print("C");
  } else {
    tft.print("--C");
  }

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(68, 64);
  tft.print(descricaoTempo(weatherCode));

  tft.drawFastHLine(0, 87, 160, tft.color565(70, 70, 70));

  tft.setCursor(6, 96);
  tft.print("Sensacao: ");
  if (!isnan(sensacao)) tft.print(String(sensacao, 0) + "C"); else tft.print("--");

  tft.setCursor(6, 108);
  tft.print("Umidade:  ");
  if (!isnan(umidade)) tft.print(String(umidade, 0) + "%"); else tft.print("--");

  tft.setCursor(92, 96);
  tft.print("Vento:");
  tft.setCursor(92, 108);
  if (!isnan(vento)) tft.print(String(vento, 0) + " km/h"); else tft.print("--");

  tft.setTextColor(tft.color565(130, 130, 130));
  tft.setCursor(6, 120);
  tft.print("Atualizado ");
  tft.print(horaDaConsulta());
}

bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED || !coordenadasValidas) return false;

  cabecalho("CLIMA");
  textoLinha(42, "Atualizando previsao...");

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,is_day,wind_speed_10m" +
               "&timezone=auto";

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) {
    telaErro("CLIMA", "Falha ao iniciar HTTP");
    return false;
  }

  http.setTimeout(12000);
  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Weather HTTP: ");
    Serial.println(codigo);
    http.end();
    telaErro("CLIMA", "HTTP " + String(codigo));
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.print("Weather JSON: ");
    Serial.println(erro.c_str());
    telaErro("CLIMA", "Resposta JSON invalida");
    return false;
  }

  temperatura = doc["current"]["temperature_2m"] | NAN;
  sensacao = doc["current"]["apparent_temperature"] | NAN;
  umidade = doc["current"]["relative_humidity_2m"] | NAN;
  vento = doc["current"]["wind_speed_10m"] | NAN;
  weatherCode = doc["current"]["weather_code"] | -1;
  isDay = (doc["current"]["is_day"] | 1) == 1;
  weatherTime = String((const char*)(doc["current"]["time"] | ""));

  Serial.print("Clima: ");
  Serial.print(temperatura);
  Serial.print(" C, umidade ");
  Serial.print(umidade);
  Serial.print("%, codigo ");
  Serial.println(weatherCode);

  mostrarClima();
  return true;
}

// ============================================================
// Botao FLASH: segure 5 s para abrir configuracao
// ============================================================

void verificarBotaoConfig() {
  if (digitalRead(CONFIG_BUTTON) != LOW) return;

  unsigned long inicio = millis();
  bool exibiuAviso = false;

  while (digitalRead(CONFIG_BUTTON) == LOW) {
    unsigned long tempo = millis() - inicio;

    if (!exibiuAviso && tempo > 500) {
      cabecalho("CONFIG");
      textoLinha(42, "Continue segurando FLASH");
      exibiuAviso = true;
    }

    if (tempo >= BUTTON_HOLD_TIME) {
      textoLinha(65, "Abrindo portal...", ST77XX_YELLOW, 1);
      delay(500);
      abrirPortalConfiguracao();
      ultimaConsulta = 0;
      buscarClima();
      return;
    }

    delay(50);
    yield();
  }

  if (exibiuAviso) mostrarClima();
}

// ============================================================
// Setup / Loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(CONFIG_BUTTON, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1); // 160x128 paisagem
  tft.setTextWrap(false);
  telaLimpa();

  cabecalho("CLIMABOX");
  textoLinha(42, "Iniciando...");

  if (!LittleFS.begin()) {
    Serial.println("Falha ao iniciar LittleFS");
    telaErro("ERRO", "LittleFS indisponivel");
    delay(1500);
  } else {
    carregarConfig();
  }

  // Regra principal: nunca fica indefinidamente tentando Wi-Fi.
  // Sem credencial salva, ou sem conexao em 12 s, abre o portal.
  if (!conectarWifiSalvo()) {
    if (!abrirPortalConfiguracao()) {
      telaErro("SEM WIFI", "Reinicie para tentar de novo");
      return;
    }
  }

  if (!coordenadasValidas) {
    if (!geocodificarCidade()) {
      telaErro("CIDADE", "Segure FLASH por 5 s");
      return;
    }
  }

  buscarClima();
  ultimaConsulta = millis();
}

void loop() {
  verificarBotaoConfig();

  unsigned long agora = millis();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimaTentativaWifi = 0;
    if (agora - ultimaTentativaWifi > 30000UL) {
      ultimaTentativaWifi = agora;
      WiFi.reconnect();
    }
  }

  if (agora - ultimaConsulta >= WEATHER_INTERVAL) {
    ultimaConsulta = agora;
    if (WiFi.status() == WL_CONNECTED) {
      buscarClima();
    }
  }

  delay(20);
}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
extern "C" {
  #include <user_interface.h>
}

// ============================================================
// ClimaBox - NodeMCU ESP8266 + TFT ST7735 1.8" 128x160
// Versao com duas telas: CLIMA e OLHOS.
// ============================================================

#define TFT_CS   D8
#define TFT_RST  D1
#define TFT_DC   D2
#define CONFIG_BUTTON 0

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const unsigned long EYES_FRAME_INTERVAL = 90UL;

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
bool dadosValidos = false;

unsigned long ultimaConsulta = 0;
unsigned long ultimoFrameOlhos = 0;

enum TelaModo : uint32_t { MODO_CLIMA = 0, MODO_OLHOS = 1 };

struct EstadoRtc {
  uint32_t magic;
  uint32_t modo;
  uint32_t checksum;
};

const uint32_t RTC_MAGIC = 0x434C494D;
const uint32_t RTC_SLOT = 64;
TelaModo modoAtual = MODO_CLIMA;

uint32_t checksumRtc(const EstadoRtc& e) { return e.magic ^ e.modo ^ 0xA55AA55A; }

bool lerEstadoRtc(EstadoRtc& e) {
  if (!system_rtc_mem_read(RTC_SLOT, &e, sizeof(e))) return false;
  return e.magic == RTC_MAGIC && e.checksum == checksumRtc(e) && e.modo <= 1;
}

void salvarModoRtc(TelaModo modo) {
  EstadoRtc e;
  e.magic = RTC_MAGIC;
  e.modo = (uint32_t)modo;
  e.checksum = checksumRtc(e);
  system_rtc_mem_write(RTC_SLOT, &e, sizeof(e));
}

void determinarModoInicial() {
  const rst_info* info = ESP.getResetInfoPtr();
  EstadoRtc e;
  bool rtcOk = lerEstadoRtc(e);
  if (!info || info->reason == REASON_DEFAULT_RST || !rtcOk) {
    modoAtual = MODO_CLIMA;
    salvarModoRtc(modoAtual);
    return;
  }
  if (info->reason == REASON_EXT_SYS_RST) {
    modoAtual = (e.modo == MODO_CLIMA) ? MODO_OLHOS : MODO_CLIMA;
    salvarModoRtc(modoAtual);
    return;
  }
  modoAtual = (TelaModo)e.modo;
}

void telaLimpa(uint16_t cor = ST77XX_BLACK) { tft.fillScreen(cor); }

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

bool salvarConfig() {
  DynamicJsonDocument doc(512);
  doc["cidade"] = cidade;
  doc["uf"] = uf;
  doc["latitude"] = latitude;
  doc["longitude"] = longitude;
  doc["coords_ok"] = coordenadasValidas;
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void carregarConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) return;
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(512);
  DeserializationError erro = deserializeJson(doc, f);
  f.close();
  if (erro) return;
  cidade = doc["cidade"] | "Manaus";
  uf = doc["uf"] | "AM";
  latitude = doc["latitude"] | -3.1190f;
  longitude = doc["longitude"] | -60.0217f;
  coordenadasValidas = doc["coords_ok"] | true;
}

String urlEncode(const String& value) {
  String encoded;
  char hex[4];
  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') encoded += (char)c;
    else { snprintf(hex, sizeof(hex), "%%%02X", c); encoded += hex; }
  }
  return encoded;
}

void mostrarPortalNaTela() {
  cabecalho("CONFIG WIFI", ST77XX_YELLOW);
  textoLinha(38, "Conecte o celular em:");
  textoLinha(55, AP_SSID, ST77XX_CYAN, 1);
  textoLinha(74, "Senha:");
  textoLinha(88, AP_PASSWORD, ST77XX_YELLOW, 1);
  textoLinha(108, "Depois abra:");
  textoLinha(122, "192.168.4.1", ST77XX_GREEN, 2);
}

void callbackAP(WiFiManager* wm) { mostrarPortalNaTela(); }

bool geocodificarCidade() {
  if (WiFi.status() != WL_CONNECTED) return false;
  cabecalho("LOCAL");
  textoLinha(40, "Localizando cidade...");
  textoLinha(58, cidade + " - " + uf, ST77XX_YELLOW, 1);
  String busca = cidade;
  if (uf.length()) busca += ", " + uf;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(busca) +
               "&count=1&language=pt&format=json&countryCode=BR";
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);
  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, payload)) return false;
  JsonArray resultados = doc["results"].as<JsonArray>();
  if (resultados.isNull() || resultados.size() == 0) return false;
  JsonObject local = resultados[0];
  latitude = local["latitude"].as<float>();
  longitude = local["longitude"].as<float>();
  const char* nome = local["name"] | nullptr;
  if (nome && strlen(nome)) cidade = nome;
  coordenadasValidas = true;
  salvarConfig();
  return true;
}

bool abrirPortalConfiguracao() {
  WiFiManager wm;
  wm.setAPCallback(callbackAP);
  wm.setConfigPortalTimeout(0);
  wm.setMinimumSignalQuality(8);
  wm.setRemoveDuplicateAPs(true);
  char cidadeBuffer[41] = {0};
  char ufBuffer[5] = {0};
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
  if (!ok || WiFi.status() != WL_CONNECTED) return false;
  String novaCidade = campoCidade.getValue();
  String novaUf = campoUF.getValue();
  novaCidade.trim();
  novaUf.trim();
  novaUf.toUpperCase();
  if (novaCidade.length()) cidade = novaCidade;
  if (novaUf.length()) uf = novaUf;
  coordenadasValidas = false;
  salvarConfig();
  if (!geocodificarCidade()) {
    telaErro("CIDADE", "Nao encontrada");
    delay(2500);
    return false;
  }
  return true;
}

bool haWifiSalvo() { return WiFi.SSID().length() > 0; }

bool conectarWifiSalvo() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  if (!haWifiSalvo()) return false;
  cabecalho("CLIMABOX");
  textoLinha(38, "Conectando Wi-Fi...");
  textoLinha(55, WiFi.SSID(), ST77XX_YELLOW, 1);
  WiFi.begin();
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_CONNECT_TIMEOUT) {
    int restante = (int)((WIFI_CONNECT_TIMEOUT - (millis() - inicio) + 999) / 1000);
    tft.fillRect(6, 78, 150, 12, ST77XX_BLACK);
    textoLinha(78, "Tentativa: " + String(restante) + "s");
    delay(250);
    yield();
  }
  return WiFi.status() == WL_CONNECTED;
}

String descricaoTempo(int codigo) {
  if (codigo == 0) return "Ceu limpo";
  if (codigo == 1) return "Quase limpo";
  if (codigo == 2) return "Parc. nublado";
  if (codigo == 3) return "Nublado";
  if (codigo == 45 || codigo == 48) return "Neblina";
  if (codigo >= 51 && codigo <= 57) return "Garoa";
  if (codigo >= 61 && codigo <= 67) return "Chuva";
  if (codigo >= 80 && codigo <= 82) return "Pancadas";
  if (codigo >= 95) return "Trovoadas";
  return "Tempo variavel";
}

void desenharIconeClima(int code, int cx, int cy) {
  uint16_t amarelo = ST77XX_YELLOW, branco = ST77XX_WHITE, azul = tft.color565(80, 170, 255);
  if (code == 0 || code == 1) {
    tft.fillCircle(cx, cy, 10, amarelo);
    for (int a = 0; a < 8; a++) {
      float ang = a * PI / 4.0f;
      tft.drawLine(cx + cos(ang)*14, cy + sin(ang)*14, cx + cos(ang)*19, cy + sin(ang)*19, amarelo);
    }
  } else {
    tft.fillCircle(cx - 8, cy + 2, 8, branco);
    tft.fillCircle(cx + 2, cy - 4, 11, branco);
    tft.fillCircle(cx + 13, cy + 3, 8, branco);
    tft.fillRect(cx - 16, cy + 2, 38, 10, branco);
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95)
      for (int i = -10; i <= 12; i += 8) tft.drawLine(cx+i, cy+17, cx+i-3, cy+24, azul);
  }
}

void desenharTelaClima() {
  telaLimpa();
  tft.setTextWrap(false);
  String local = cidade;
  if (uf.length()) local += "-" + uf;
  if (local.length() > 17) local = local.substring(0, 17);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.print(local);
  desenharIconeClima(weatherCode, 30, 57);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(4);
  tft.setCursor(58, 40);
  if (dadosValidos) { tft.print(temperatura, 0); tft.print("C"); } else tft.print("--C");
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(6, 92);
  tft.print(descricaoTempo(weatherCode));
  tft.setCursor(6, 108);
  tft.print("Sensacao: ");
  if (dadosValidos) tft.print(sensacao, 0); else tft.print("--");
  tft.print(" C");
  tft.setCursor(6, 122);
  tft.print("Umidade: ");
  if (dadosValidos) tft.print(umidade, 0); else tft.print("--");
  tft.print("%");
  tft.setCursor(6, 136);
  tft.print("Vento: ");
  if (dadosValidos) tft.print(vento, 0); else tft.print("--");
  tft.print(" km/h");
  tft.setTextColor(tft.color565(110, 110, 110));
  tft.setCursor(6, 151);
  tft.print("RST: alternar tela");
}

bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED || !coordenadasValidas) return false;
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code&timezone=auto";
  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);
  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();
  DynamicJsonDocument doc(3072);
  if (deserializeJson(doc, payload)) return false;
  temperatura = doc["current"]["temperature_2m"] | NAN;
  sensacao = doc["current"]["apparent_temperature"] | NAN;
  umidade = doc["current"]["relative_humidity_2m"] | NAN;
  vento = doc["current"]["wind_speed_10m"] | NAN;
  weatherCode = doc["current"]["weather_code"] | -1;
  dadosValidos = !isnan(temperatura);
  if (modoAtual == MODO_CLIMA) desenharTelaClima();
  return dadosValidos;
}

// ============================================================
// Olhos animados suaves, com redesenho minimo
// ============================================================
int olhoOffsetX = 0, olhoOffsetY = 0, alvoX = 0, alvoY = 0;
int antigoX = 0, antigoY = 0;
int piscarFrame = 0;
unsigned long proximoOlhar = 0, proximaPiscada = 0, ultimoMovimentoOlho = 0;
const int OLHO_CY = 73, OLHO1_CX = 45, OLHO2_CX = 115, ABERTURA_NORMAL = 38;

void desenharBaseOlho(int cx, int cy, int abertura) {
  uint16_t branco = tft.color565(245, 245, 245);
  int h = max(2, abertura);
  tft.fillRect(cx - 31, cy - 22, 62, 44, ST77XX_BLACK);
  tft.fillRoundRect(cx - 29, cy - h/2, 58, h, min(14, h/2), branco);
}

void desenharPupila(int cx, int cy, int offsetX, int offsetY) {
  uint16_t azul = tft.color565(30, 150, 255);
  int px = cx + constrain(offsetX, -10, 10);
  int py = cy + constrain(offsetY, -5, 5);
  tft.fillCircle(px, py, 10, azul);
  tft.fillCircle(px, py, 5, ST77XX_BLACK);
  tft.fillCircle(px - 3, py - 3, 2, ST77XX_WHITE);
}

void restaurarPupilaAnterior(int cx, int cy, int offsetX, int offsetY) {
  uint16_t branco = tft.color565(245, 245, 245);
  int px = cx + constrain(offsetX, -10, 10);
  int py = cy + constrain(offsetY, -5, 5);
  tft.fillCircle(px, py, 12, branco);
}

void prepararTelaOlhos() {
  telaLimpa();
  tft.setTextWrap(false);
  tft.setTextColor(tft.color565(75, 75, 75));
  tft.setTextSize(1);
  tft.setCursor(23, 149);
  tft.print("RST: mostrar clima");
  olhoOffsetX = olhoOffsetY = alvoX = alvoY = antigoX = antigoY = 0;
  piscarFrame = 0;
  desenharBaseOlho(OLHO1_CX, OLHO_CY, ABERTURA_NORMAL);
  desenharBaseOlho(OLHO2_CX, OLHO_CY, ABERTURA_NORMAL);
  desenharPupila(OLHO1_CX, OLHO_CY, 0, 0);
  desenharPupila(OLHO2_CX, OLHO_CY, 0, 0);
  unsigned long agora = millis();
  proximoOlhar = agora + random(2000, 5000);
  proximaPiscada = agora + random(3000, 7000);
  ultimoMovimentoOlho = agora;
}

void atualizarOlhos() {
  unsigned long agora = millis();
  if (agora - ultimoFrameOlhos < EYES_FRAME_INTERVAL) return;
  ultimoFrameOlhos = agora;

  if (agora >= proximoOlhar && piscarFrame == 0) {
    alvoX = random(-8, 9);
    alvoY = random(-3, 4);
    proximoOlhar = agora + random(2200, 5500);
  }

  if (piscarFrame > 0) {
    static const uint8_t piscada[] = {38, 30, 20, 10, 3, 3, 10, 20, 30, 38};
    int abertura = piscada[piscarFrame - 1];
    desenharBaseOlho(OLHO1_CX, OLHO_CY, abertura);
    desenharBaseOlho(OLHO2_CX, OLHO_CY, abertura);
    if (abertura > 15) {
      desenharPupila(OLHO1_CX, OLHO_CY, olhoOffsetX, olhoOffsetY);
      desenharPupila(OLHO2_CX, OLHO_CY, olhoOffsetX, olhoOffsetY);
    }
    piscarFrame++;
    if (piscarFrame > 10) {
      piscarFrame = 0;
      proximaPiscada = agora + random(3000, 7000);
    }
    return;
  }

  if (agora >= proximaPiscada) { piscarFrame = 1; return; }

  bool mudou = false;
  if (agora - ultimoMovimentoOlho >= 120) {
    ultimoMovimentoOlho = agora;
    antigoX = olhoOffsetX;
    antigoY = olhoOffsetY;
    if (olhoOffsetX < alvoX) { olhoOffsetX++; mudou = true; }
    else if (olhoOffsetX > alvoX) { olhoOffsetX--; mudou = true; }
    if (olhoOffsetY < alvoY) { olhoOffsetY++; mudou = true; }
    else if (olhoOffsetY > alvoY) { olhoOffsetY--; mudou = true; }
  }

  if (mudou) {
    restaurarPupilaAnterior(OLHO1_CX, OLHO_CY, antigoX, antigoY);
    restaurarPupilaAnterior(OLHO2_CX, OLHO_CY, antigoX, antigoY);
    desenharPupila(OLHO1_CX, OLHO_CY, olhoOffsetX, olhoOffsetY);
    desenharPupila(OLHO2_CX, OLHO_CY, olhoOffsetX, olhoOffsetY);
  }
}

void verificarBotaoConfig() {
  if (digitalRead(CONFIG_BUTTON) != LOW) return;
  unsigned long inicio = millis();
  while (digitalRead(CONFIG_BUTTON) == LOW) {
    if (millis() - inicio >= BUTTON_HOLD_TIME) {
      cabecalho("CONFIG");
      textoLinha(45, "Abrindo portal...");
      delay(500);
      abrirPortalConfiguracao();
      ESP.restart();
      return;
    }
    delay(20);
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  telaLimpa();
  randomSeed(ESP.getCycleCount());
  determinarModoInicial();
  if (!LittleFS.begin()) {
    telaErro("ERRO", "LittleFS falhou");
    delay(1500);
  }
  carregarConfig();
  bool conectado = conectarWifiSalvo();
  if (!conectado) {
    if (!abrirPortalConfiguracao()) {
      telaErro("WIFI", "Falha na configuracao");
      delay(2000);
      ESP.restart();
    }
  }
  if (!coordenadasValidas) geocodificarCidade();
  buscarClima();
  ultimaConsulta = millis();
  if (modoAtual == MODO_OLHOS) prepararTelaOlhos();
  else desenharTelaClima();
}

void loop() {
  verificarBotaoConfig();
  unsigned long agora = millis();
  if (agora - ultimaConsulta >= WEATHER_INTERVAL) {
    ultimaConsulta = agora;
    if (WiFi.status() == WL_CONNECTED) buscarClima();
  }
  if (modoAtual == MODO_OLHOS) atualizarOlhos();
  yield();
}

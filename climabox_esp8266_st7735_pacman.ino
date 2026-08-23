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
// Tela 1: clima
// Tela 2: animacao Pac-Man + fantasmas
// RST alterna entre as telas.
// FLASH pressionado por 5s abre configuracao Wi-Fi/cidade.
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
const unsigned long PACMAN_FRAME_INTERVAL = 85UL;

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
unsigned long ultimoFramePacman = 0;

// ============================================================
// Alternancia por RST usando RTC RAM
// ============================================================

enum TelaModo : uint32_t {
  MODO_CLIMA = 0,
  MODO_PACMAN = 1
};

struct EstadoRtc {
  uint32_t magic;
  uint32_t modo;
  uint32_t checksum;
};

const uint32_t RTC_MAGIC = 0x434C494D;
const uint32_t RTC_SLOT = 64;
TelaModo modoAtual = MODO_CLIMA;

uint32_t checksumRtc(const EstadoRtc& e) {
  return e.magic ^ e.modo ^ 0xA55AA55A;
}

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
    modoAtual = (e.modo == MODO_CLIMA) ? MODO_PACMAN : MODO_CLIMA;
    salvarModoRtc(modoAtual);
    return;
  }

  modoAtual = (TelaModo)e.modo;
}

// ============================================================
// Tela / helpers
// ============================================================

void telaLimpa(uint16_t cor = ST77XX_BLACK) {
  tft.fillScreen(cor);
}

void cabecalho(const String& texto, uint16_t cor = ST77XX_CYAN) {
  telaLimpa();
  tft.setTextWrap(false);
  tft.setTextColor(cor);
  tft.setTextSize(2);
  tft.setCursor(5, 4);
  tft.print(texto);
  tft.drawFastHLine(0, 23, 160, tft.color565(60, 60, 60));
}

void textoLinha(int y, const String& texto, uint16_t cor = ST77XX_WHITE, uint8_t tamanho = 1) {
  tft.setTextColor(cor);
  tft.setTextSize(tamanho);
  tft.setCursor(5, y);
  tft.print(texto);
}

void telaErro(const String& titulo, const String& detalhe) {
  cabecalho(titulo, ST77XX_RED);
  textoLinha(38, detalhe);
}

// ============================================================
// Persistencia
// ============================================================

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
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      encoded += hex;
    }
  }
  return encoded;
}

// ============================================================
// Wi-Fi / configuracao
// ============================================================

void mostrarPortalNaTela() {
  cabecalho("CONFIG WIFI", ST77XX_YELLOW);
  textoLinha(34, "Rede: " + String(AP_SSID), ST77XX_CYAN);
  textoLinha(50, "Senha: " + String(AP_PASSWORD), ST77XX_YELLOW);
  textoLinha(71, "No celular abra:");
  textoLinha(87, "192.168.4.1", ST77XX_GREEN, 2);
}

void callbackAP(WiFiManager* wm) {
  mostrarPortalNaTela();
}

bool geocodificarCidade() {
  if (WiFi.status() != WL_CONNECTED) return false;

  cabecalho("LOCAL");
  textoLinha(38, "Localizando cidade...");
  textoLinha(56, cidade + " - " + uf, ST77XX_YELLOW);

  String busca = cidade;
  if (uf.length()) busca += ", " + uf;

  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" +
               urlEncode(busca) +
               "&count=1&language=pt&format=json&countryCode=BR";

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    http.end();
    return false;
  }

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

bool haWifiSalvo() {
  return WiFi.SSID().length() > 0;
}

bool conectarWifiSalvo() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  if (!haWifiSalvo()) return false;

  cabecalho("CLIMABOX");
  textoLinha(38, "Conectando Wi-Fi...");
  textoLinha(55, WiFi.SSID(), ST77XX_YELLOW);
  WiFi.begin();

  unsigned long inicio = millis();
  int ultimoRestante = -1;

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_CONNECT_TIMEOUT) {
    int restante = (int)((WIFI_CONNECT_TIMEOUT - (millis() - inicio) + 999) / 1000);
    if (restante != ultimoRestante) {
      ultimoRestante = restante;
      tft.fillRect(5, 75, 150, 14, ST77XX_BLACK);
      textoLinha(76, "Tentativa: " + String(restante) + "s");
    }
    delay(100);
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

// ============================================================
// Clima
// ============================================================

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
  uint16_t amarelo = ST77XX_YELLOW;
  uint16_t branco = ST77XX_WHITE;
  uint16_t azul = tft.color565(80, 170, 255);

  if (code == 0 || code == 1) {
    tft.fillCircle(cx, cy, 9, amarelo);
    for (int a = 0; a < 8; a++) {
      float ang = a * PI / 4.0f;
      tft.drawLine(cx + cos(ang) * 12, cy + sin(ang) * 12,
                   cx + cos(ang) * 17, cy + sin(ang) * 17, amarelo);
    }
  } else {
    tft.fillCircle(cx - 8, cy + 2, 7, branco);
    tft.fillCircle(cx + 1, cy - 4, 10, branco);
    tft.fillCircle(cx + 11, cy + 3, 7, branco);
    tft.fillRect(cx - 14, cy + 2, 32, 9, branco);
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95) {
      for (int i = -8; i <= 10; i += 7)
        tft.drawLine(cx + i, cy + 15, cx + i - 2, cy + 21, azul);
    }
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
  tft.setCursor(4, 3);
  tft.print(local);

  desenharIconeClima(weatherCode, 27, 48);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(4);
  tft.setCursor(54, 34);
  if (dadosValidos) {
    tft.print(temperatura, 0);
    tft.print("C");
  } else {
    tft.print("--C");
  }

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 76);
  tft.print(descricaoTempo(weatherCode));

  tft.setCursor(5, 91);
  tft.print("Sens: ");
  if (dadosValidos) tft.print(sensacao, 0); else tft.print("--");
  tft.print("C   Umid: ");
  if (dadosValidos) tft.print(umidade, 0); else tft.print("--");
  tft.print("%");

  tft.setCursor(5, 106);
  tft.print("Vento: ");
  if (dadosValidos) tft.print(vento, 0); else tft.print("--");
  tft.print(" km/h");

  tft.setTextColor(tft.color565(100, 100, 100));
  tft.setCursor(5, 119);
  tft.print("RST: Pac-Man");
}

bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED || !coordenadasValidas) return false;

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code" +
               "&timezone=auto";

  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

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
// Pac-Man + fantasmas
// Redesenho apenas da faixa animada para evitar flicker.
// ============================================================

int pacX = -15;
int passoAnim = 0;
bool bocaAberta = true;

const int PISTA_Y = 60;
const int PAC_R = 11;
const int GHOST_Y = 60;
const int GHOST_W = 20;
const int GHOST_H = 22;

void desenharLabirinto() {
  telaLimpa();

  uint16_t azulMaze = tft.color565(35, 70, 255);
  tft.drawRect(2, 31, 156, 64, azulMaze);
  tft.drawRect(5, 34, 150, 58, azulMaze);

  tft.drawFastHLine(8, 46, 42, azulMaze);
  tft.drawFastHLine(110, 46, 42, azulMaze);
  tft.drawFastHLine(8, 80, 42, azulMaze);
  tft.drawFastHLine(110, 80, 42, azulMaze);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print("PAC-MAN");

  tft.setTextColor(tft.color565(100, 100, 100));
  tft.setCursor(5, 116);
  tft.print("RST: mostrar clima");

  // Pellets fixos da pista.
  for (int x = 12; x < 152; x += 14) {
    tft.fillCircle(x, PISTA_Y, 2, ST77XX_WHITE);
  }

  // Power pellets.
  tft.fillCircle(12, PISTA_Y, 4, ST77XX_WHITE);
  tft.fillCircle(148, PISTA_Y, 4, ST77XX_WHITE);
}

void desenharPacman(int x, int y, bool aberta) {
  tft.fillCircle(x, y, PAC_R, ST77XX_YELLOW);

  if (aberta) {
    // Boca triangular voltada para a direita.
    tft.fillTriangle(x + 2, y,
                     x + PAC_R + 2, y - 8,
                     x + PAC_R + 2, y + 8,
                     ST77XX_BLACK);
  } else {
    tft.drawFastHLine(x + 2, y, PAC_R, ST77XX_BLACK);
  }

  tft.fillCircle(x + 2, y - 5, 1, ST77XX_BLACK);
}

void desenharFantasma(int x, int y, uint16_t cor, int olharDir = 1) {
  // Corpo: topo arredondado + parte inferior retangular.
  tft.fillCircle(x, y - 5, GHOST_W / 2, cor);
  tft.fillRect(x - GHOST_W / 2, y - 5, GHOST_W, 15, cor);

  // Base serrilhada.
  tft.fillTriangle(x - 10, y + 10, x - 5, y + 5, x, y + 10, cor);
  tft.fillTriangle(x, y + 10, x + 5, y + 5, x + 10, y + 10, cor);

  // Olhos.
  tft.fillCircle(x - 4, y - 6, 4, ST77XX_WHITE);
  tft.fillCircle(x + 4, y - 6, 4, ST77XX_WHITE);

  uint16_t azulOlho = tft.color565(30, 70, 255);
  int dx = olharDir >= 0 ? 1 : -1;
  tft.fillCircle(x - 4 + dx, y - 6, 2, azulOlho);
  tft.fillCircle(x + 4 + dx, y - 6, 2, azulOlho);
}

void limparSprite(int cx, int cy, int w, int h) {
  tft.fillRect(cx - w / 2 - 2, cy - h / 2 - 2, w + 5, h + 5, ST77XX_BLACK);
}

void restaurarPelletsNaArea(int x0, int x1) {
  for (int x = 12; x < 152; x += 14) {
    if (x >= x0 && x <= x1 && x > pacX + 8) {
      int r = (x == 12 || x == 148) ? 4 : 2;
      tft.fillCircle(x, PISTA_Y, r, ST77XX_WHITE);
    }
  }
}

void prepararTelaPacman() {
  desenharLabirinto();
  pacX = -15;
  passoAnim = 0;
  bocaAberta = true;
  ultimoFramePacman = millis();
}

void atualizarPacman() {
  unsigned long agora = millis();
  if (agora - ultimoFramePacman < PACMAN_FRAME_INTERVAL) return;
  ultimoFramePacman = agora;

  int pacAnt = pacX;
  int fant1Ant = pacAnt - 34;
  int fant2Ant = pacAnt - 60;
  int fant3Ant = pacAnt - 86;

  // Apaga somente os sprites anteriores.
  limparSprite(pacAnt, PISTA_Y, 27, 27);
  limparSprite(fant1Ant, GHOST_Y, 25, 29);
  limparSprite(fant2Ant, GHOST_Y, 25, 29);
  limparSprite(fant3Ant, GHOST_Y, 25, 29);

  // Restaura apenas pellets que ainda estao na frente do Pac-Man.
  restaurarPelletsNaArea(pacAnt - 100, pacAnt + 20);

  pacX += 3;
  passoAnim++;
  bocaAberta = ((passoAnim / 2) % 2) == 0;

  if (pacX > 190) {
    pacX = -15;
    desenharLabirinto();
  }

  int fant1X = pacX - 34;
  int fant2X = pacX - 60;
  int fant3X = pacX - 86;

  // Fantasmas ficam atras do Pac-Man, como numa perseguicao.
  if (fant3X > -15 && fant3X < 175)
    desenharFantasma(fant3X, GHOST_Y, tft.color565(255, 70, 70));
  if (fant2X > -15 && fant2X < 175)
    desenharFantasma(fant2X, GHOST_Y, tft.color565(255, 120, 210));
  if (fant1X > -15 && fant1X < 175)
    desenharFantasma(fant1X, GHOST_Y, tft.color565(70, 220, 255));

  if (pacX > -15 && pacX < 175)
    desenharPacman(pacX, PISTA_Y, bocaAberta);
}

// ============================================================
// FLASH por 5 s: configuracao
// ============================================================

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

// ============================================================
// Setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  telaLimpa();

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

  if (modoAtual == MODO_PACMAN) prepararTelaPacman();
  else desenharTelaClima();
}

void loop() {
  verificarBotaoConfig();

  unsigned long agora = millis();
  if (agora - ultimaConsulta >= WEATHER_INTERVAL) {
    ultimaConsulta = agora;
    if (WiFi.status() == WL_CONNECTED) buscarClima();
  }

  if (modoAtual == MODO_PACMAN) atualizarPacman();

  yield();
}

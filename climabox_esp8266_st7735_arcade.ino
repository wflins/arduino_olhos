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
// ClimaBox Arcade - NodeMCU ESP8266 + ST7735 1.8" 160x128
//
// RST alterna:
// CLIMA -> PAC-MAN -> PITFALL -> ENDURO -> RIVER RIDER -> CLIMA
//
// FLASH pressionado por 5 segundos abre configuracao Wi-Fi/cidade.
// ============================================================

#define TFT_CS D8
#define TFT_RST D1
#define TFT_DC D2
#define CONFIG_BUTTON 0

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const unsigned long GAME_FRAME_INTERVAL = 55UL; // ~18 FPS, estavel no ESP8266/ST7735
const unsigned long WIFI_RETRY_INTERVAL = 30000UL;

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
unsigned long ultimoFrameJogo = 0;
unsigned long ultimaTentativaWifi = 0;
bool primeiraConsultaFeita = false;

// ============================================================
// Modos / alternancia por RST usando RTC RAM
// ============================================================

enum TelaModo : uint32_t {
  MODO_CLIMA = 0,
  MODO_PACMAN = 1,
  MODO_PITFALL = 2,
  MODO_ENDURO = 3,
  MODO_RIVER = 4
};

const uint32_t TOTAL_MODOS = 5;

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
  return e.magic == RTC_MAGIC && e.checksum == checksumRtc(e) && e.modo < TOTAL_MODOS;
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

  // Ligou a alimentacao: inicia no clima.
  if (!info || info->reason == REASON_DEFAULT_RST || !rtcOk) {
    modoAtual = MODO_CLIMA;
    salvarModoRtc(modoAtual);
    return;
  }

  // Clique no RST: avanca um modo.
  if (info->reason == REASON_EXT_SYS_RST) {
    modoAtual = (TelaModo)((e.modo + 1) % TOTAL_MODOS);
    salvarModoRtc(modoAtual);
    return;
  }

  // Reset por software/watchdog: mantem o modo.
  modoAtual = (TelaModo)e.modo;
}

// ============================================================
// Helpers de tela
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

void rodapeProximo(const String& proximo) {
  tft.fillRect(0, 114, 160, 14, ST77XX_BLACK);
  tft.setTextColor(tft.color565(100, 100, 100));
  tft.setTextSize(1);
  tft.setCursor(5, 118);
  tft.print("RST: ");
  tft.print(proximo);
}

// ============================================================
// Persistencia da cidade
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
  if (deserializeJson(doc, f)) {
    f.close();
    return;
  }
  f.close();

  cidade = doc["cidade"] | "Manaus";
  uf = doc["uf"] | "AM";
  latitude = doc["latitude"] | -3.1190f;
  longitude = doc["longitude"] | -60.0217f;
  coordenadasValidas = doc["coords_ok"] | true;
}

String urlEncode(const String& valor) {
  String saida;
  char hex[4];
  for (size_t i = 0; i < valor.length(); i++) {
    uint8_t c = (uint8_t)valor[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      saida += (char)c;
    } else {
      snprintf(hex, sizeof(hex), "%%%02X", c);
      saida += hex;
    }
  }
  return saida;
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

  latitude = resultados[0]["latitude"].as<float>();
  longitude = resultados[0]["longitude"].as<float>();
  const char* nome = resultados[0]["name"] | nullptr;
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

  WiFiManagerParameter titulo("<p><strong>Local do clima</strong></p>");
  WiFiManagerParameter campoCidade("cidade", "Cidade", cidadeBuffer, 40);
  WiFiManagerParameter campoUF("uf", "UF (ex.: AM)", ufBuffer, 4);
  wm.addParameter(&titulo);
  wm.addParameter(&campoCidade);
  wm.addParameter(&campoUF);

  mostrarPortalNaTela();
  if (!wm.startConfigPortal(AP_SSID, AP_PASSWORD) || WiFi.status() != WL_CONNECTED) return false;

  String novaCidade = campoCidade.getValue();
  String novaUf = campoUF.getValue();
  novaCidade.trim();
  novaUf.trim();
  novaUf.toUpperCase();
  if (novaCidade.length()) cidade = novaCidade;
  if (novaUf.length()) uf = novaUf;

  coordenadasValidas = false;
  salvarConfig();
  if (!geocodificarCidade()) return false;
  return true;
}

bool iniciarWifiSemBloquear() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Sem credenciais: abre o portal obrigatoriamente.
  if (!WiFi.SSID().length()) return abrirPortalConfiguracao();

  // Com credenciais: conecta em segundo plano para o RST trocar de tela rapido.
  WiFi.begin();
  ultimaTentativaWifi = millis();
  return true;
}

void manterWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - ultimaTentativaWifi < WIFI_RETRY_INTERVAL) return;
  ultimaTentativaWifi = millis();
  WiFi.disconnect();
  delay(20);
  WiFi.begin();
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
  tft.print(dadosValidos ? descricaoTempo(weatherCode) : "Conectando ao clima...");

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

  rodapeProximo("Pac-Man");
}

bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!coordenadasValidas && !geocodificarCidade()) return false;

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code" +
               "&timezone=auto";

  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);
  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
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
  primeiraConsultaFeita = dadosValidos;
  ultimaConsulta = millis();

  if (modoAtual == MODO_CLIMA) desenharTelaClima();
  return dadosValidos;
}

// ============================================================
// Canvas compartilhado para os quatro jogos
// 160 x 64 = ~20 KB. So existe um buffer para economizar RAM.
// ============================================================

GFXcanvas16 cena(160, 64);
uint32_t frameJogo = 0;
unsigned long inicioCena = 0;

void desenharTituloJogo(const String& titulo, uint16_t cor, const String& proximo) {
  telaLimpa();
  tft.setTextWrap(false);
  tft.setTextColor(cor);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print(titulo);
  rodapeProximo(proximo);
}

// ============================================================
// PAC-MAN
// ============================================================

bool pacCacador = false;
uint16_t pacScroll = 0;

void ghostCanvas(int x, int y, uint16_t cor, bool assustado = false) {
  cena.fillCircle(x, y - 4, 9, cor);
  cena.fillRect(x - 9, y - 4, 18, 13, cor);
  cena.fillTriangle(x - 9, y + 9, x - 5, y + 5, x - 1, y + 9, cor);
  cena.fillTriangle(x - 1, y + 9, x + 3, y + 5, x + 7, y + 9, cor);
  cena.fillTriangle(x + 7, y + 9, x + 9, y + 5, x + 9, y + 9, cor);
  cena.fillCircle(x - 4, y - 5, 3, ST77XX_WHITE);
  cena.fillCircle(x + 4, y - 5, 3, ST77XX_WHITE);
  uint16_t pupila = assustado ? ST77XX_WHITE : tft.color565(30, 70, 255);
  cena.fillCircle(x - 3, y - 5, 1, pupila);
  cena.fillCircle(x + 5, y - 5, 1, pupila);
}

void pacCanvas(int x, int y, bool bocaAberta) {
  cena.fillCircle(x, y, 10, ST77XX_YELLOW);
  if (bocaAberta)
    cena.fillTriangle(x + 1, y, x + 12, y - 7, x + 12, y + 7, ST77XX_BLACK);
  else
    cena.drawFastHLine(x + 2, y, 9, ST77XX_BLACK);
  cena.fillCircle(x + 2, y - 5, 1, ST77XX_BLACK);
}

void prepararPacman() {
  desenharTituloJogo("PAC-MAN", ST77XX_YELLOW, "Pitfall");
  pacCacador = false;
  pacScroll = 0;
  frameJogo = 0;
  inicioCena = millis();
}

void atualizarPacman() {
  unsigned long agora = millis();
  if (agora - inicioCena >= 6500UL) {
    pacCacador = !pacCacador;
    inicioCena = agora;
  }

  cena.fillScreen(ST77XX_BLACK);
  uint16_t azul = tft.color565(35, 70, 255);
  cena.drawFastHLine(0, 7, 160, azul);
  cena.drawFastHLine(0, 56, 160, azul);

  pacScroll += 2;
  for (int x = -16; x < 176; x += 16) {
    int xx = x - (pacScroll % 16);
    cena.fillCircle(xx, 32, 2, ST77XX_WHITE);
  }

  bool boca = ((frameJogo / 3) % 2) == 0;
  if (!pacCacador) {
    pacCanvas(128, 32, boca);
    ghostCanvas(91, 32, tft.color565(70, 220, 255));
    ghostCanvas(64, 32, tft.color565(255, 120, 210));
    ghostCanvas(37, 32, tft.color565(255, 70, 70));
  } else {
    uint16_t medo = tft.color565(45, 65, 235);
    pacCanvas(32, 32, boca);
    ghostCanvas(72, 32, medo, true);
    ghostCanvas(101, 32, medo, true);
    ghostCanvas(130, 32, medo, true);
  }

  tft.drawRGBBitmap(0, 34, cena.getBuffer(), 160, 64);
}

// ============================================================
// PITFALL - corredor na selva, troncos e lago
// ============================================================

void prepararPitfall() {
  desenharTituloJogo("PITFALL", tft.color565(70, 220, 80), "Enduro");
  frameJogo = 0;
  inicioCena = millis();
}

void atualizarPitfall() {
  cena.fillScreen(tft.color565(45, 130, 55));

  // Ceu entre as copas e solo.
  cena.fillRect(0, 0, 160, 22, tft.color565(75, 175, 210));
  cena.fillRect(0, 45, 160, 19, tft.color565(145, 90, 35));

  // Arvores se movem para a esquerda.
  int desloc = (frameJogo * 2) % 48;
  for (int x = -24; x < 190; x += 48) {
    int xx = x - desloc;
    cena.fillRect(xx + 12, 13, 6, 34, tft.color565(105, 60, 25));
    cena.fillCircle(xx + 15, 10, 13, tft.color565(20, 105, 30));
  }

  // Obstaculos em looping.
  int obst = 190 - ((frameJogo * 3) % 220);
  if (obst > -30 && obst < 180) {
    if (((frameJogo / 75) % 2) == 0) {
      cena.fillRoundRect(obst, 42, 28, 8, 4, tft.color565(95, 45, 15));
      cena.drawFastHLine(obst + 3, 44, 22, tft.color565(190, 120, 55));
    } else {
      cena.fillRect(obst, 43, 34, 11, tft.color565(25, 75, 190));
      cena.drawFastHLine(obst, 43, 34, ST77XX_WHITE);
      cena.fillCircle(obst + 9, 48, 3, tft.color565(30, 130, 50));
      cena.fillCircle(obst + 23, 48, 3, tft.color565(30, 130, 50));
    }
  }

  // Heroi: corre e pula automaticamente perto do obstaculo.
  int heroX = 37;
  int heroBaseY = 42;
  int distancia = obst - heroX;
  int salto = 0;
  if (distancia > -8 && distancia < 48) {
    float fase = (48 - distancia) / 56.0f * PI;
    salto = (int)(sin(fase) * 18.0f);
    if (salto < 0) salto = 0;
  }
  int hy = heroBaseY - salto;

  uint16_t pele = tft.color565(235, 175, 110);
  uint16_t roupa = tft.color565(245, 225, 80);
  cena.fillCircle(heroX, hy - 13, 4, pele);
  cena.fillRect(heroX - 3, hy - 9, 7, 10, roupa);
  bool perna = ((frameJogo / 3) % 2) == 0;
  if (perna) {
    cena.drawLine(heroX, hy, heroX - 6, hy + 8, ST77XX_BLACK);
    cena.drawLine(heroX + 2, hy, heroX + 7, hy + 5, ST77XX_BLACK);
  } else {
    cena.drawLine(heroX, hy, heroX - 3, hy + 6, ST77XX_BLACK);
    cena.drawLine(heroX + 2, hy, heroX + 8, hy + 8, ST77XX_BLACK);
  }

  tft.drawRGBBitmap(0, 34, cena.getBuffer(), 160, 64);
}

// ============================================================
// ENDURO - estrada pseudo-3D com carros vindo do horizonte
// ============================================================

void prepararEnduro() {
  desenharTituloJogo("ENDURO", ST77XX_WHITE, "River Rider");
  frameJogo = 0;
  inicioCena = millis();
}

void desenharCarroCanvas(int cx, int cy, uint16_t cor, int escala) {
  int w = 10 * escala;
  int h = 5 * escala;
  cena.fillRect(cx - w / 2, cy - h, w, h, cor);
  cena.fillRect(cx - w / 3, cy - h - 3 * escala, (2 * w) / 3, 3 * escala, cor);
  cena.fillRect(cx - w / 3 + 1, cy - h - 2 * escala, (2 * w) / 3 - 2, escala, tft.color565(100, 190, 235));
  cena.fillRect(cx - w / 2, cy, 2 * escala, 2 * escala, ST77XX_BLACK);
  cena.fillRect(cx + w / 2 - 2 * escala, cy, 2 * escala, 2 * escala, ST77XX_BLACK);
}

void atualizarEnduro() {
  uint16_t ceu = tft.color565(85, 150, 220);
  uint16_t grama = tft.color565(45, 140, 45);
  uint16_t asfalto = tft.color565(65, 65, 68);
  cena.fillScreen(ceu);
  cena.fillRect(0, 21, 160, 43, grama);

  // Estrada em perspectiva.
  cena.fillTriangle(76, 20, 20, 64, 140, 64, asfalto);
  cena.drawLine(76, 20, 20, 63, ST77XX_WHITE);
  cena.drawLine(84, 20, 140, 63, ST77XX_WHITE);

  // Faixas centrais deslizando em direcao ao jogador.
  int fase = (frameJogo * 3) % 18;
  for (int y = 25 + fase; y < 64; y += 18) {
    int largura = 1 + (y - 20) / 14;
    cena.fillRect(80 - largura / 2, y, largura, 6, ST77XX_YELLOW);
  }

  // Postes laterais criam sensacao de velocidade.
  int posteFase = (frameJogo * 4) % 24;
  for (int y = 23 + posteFase; y < 64; y += 24) {
    int dx = (y - 20) * 3 / 2;
    cena.drawFastVLine(80 - dx, y - 5, 8, ST77XX_WHITE);
    cena.drawFastVLine(80 + dx, y - 5, 8, ST77XX_WHITE);
  }

  // Carros adversarios descem do horizonte e alternam de faixa.
  int ciclo = (frameJogo * 2) % 90;
  int enemyY = 22 + ciclo / 2;
  if (enemyY < 57) {
    int faixa = ((frameJogo / 90) % 3) - 1;
    int abertura = max(4, (enemyY - 18) / 3);
    int enemyX = 80 + faixa * abertura;
    int escala = enemyY > 43 ? 2 : 1;
    desenharCarroCanvas(enemyX, enemyY, tft.color565(230, 55, 55), escala);
  }

  // Carro do jogador fixo embaixo.
  desenharCarroCanvas(80, 58, tft.color565(230, 230, 235), 2);

  tft.drawRGBBitmap(0, 34, cena.getBuffer(), 160, 64);
}

// ============================================================
// RIVER RIDER - aviao sobre rio com margens rolando
// ============================================================

void prepararRiver() {
  desenharTituloJogo("RIVER RIDER", tft.color565(80, 180, 255), "Clima");
  frameJogo = 0;
  inicioCena = millis();
}

void desenharAviaoCanvas(int x, int y) {
  uint16_t prata = tft.color565(215, 220, 225);
  cena.fillTriangle(x, y - 9, x - 4, y + 8, x + 4, y + 8, prata);
  cena.fillRect(x - 12, y, 24, 4, prata);
  cena.fillRect(x - 7, y + 6, 14, 3, prata);
  cena.fillRect(x - 1, y - 5, 3, 5, tft.color565(80, 160, 225));
}

void atualizarRiver() {
  uint16_t agua = tft.color565(25, 90, 205);
  uint16_t margem = tft.color565(55, 150, 55);
  uint16_t areia = tft.color565(190, 175, 85);
  cena.fillScreen(margem);

  // Rio sinuoso: desenhado linha a linha, deslocando o padrao para baixo.
  int scroll = (frameJogo * 2) % 80;
  for (int y = 0; y < 64; y++) {
    float onda = sin((y + scroll) * 0.11f);
    int centro = 80 + (int)(onda * 18.0f);
    int meia = 35 + (int)(sin((y + scroll) * 0.07f) * 5.0f);
    cena.drawFastHLine(centro - meia - 3, y, 3, areia);
    cena.drawFastHLine(centro - meia, y, meia * 2, agua);
    cena.drawFastHLine(centro + meia, y, 3, areia);
  }

  // Barco/inimigo vindo de cima.
  int enemyY = (frameJogo * 2) % 82 - 12;
  int enemyX = 80 + (int)(sin((frameJogo * 2) * 0.11f) * 14.0f);
  if (enemyY >= 0 && enemyY < 58) {
    uint16_t vermelho = tft.color565(230, 60, 50);
    cena.fillTriangle(enemyX, enemyY + 7, enemyX - 6, enemyY - 5, enemyX + 6, enemyY - 5, vermelho);
    cena.fillRect(enemyX - 3, enemyY - 7, 6, 4, ST77XX_WHITE);
  }

  // Pequenas ilhas/obstaculos descendo.
  int ilhaY = (frameJogo * 3 + 35) % 100 - 18;
  if (ilhaY >= -5 && ilhaY < 64) {
    int ilhaX = 65 + (int)(sin(frameJogo * 0.08f) * 12.0f);
    cena.fillCircle(ilhaX, ilhaY, 5, tft.color565(70, 150, 55));
    cena.fillCircle(ilhaX + 4, ilhaY + 2, 4, tft.color565(70, 150, 55));
  }

  // Aviao do jogador fixo embaixo, com leve oscilacao lateral.
  int planeX = 80 + (int)(sin(frameJogo * 0.08f) * 10.0f);
  desenharAviaoCanvas(planeX, 49);

  tft.drawRGBBitmap(0, 34, cena.getBuffer(), 160, 64);
}

// ============================================================
// Jogos: preparar / atualizar modo corrente
// ============================================================

void prepararModoAtual() {
  ultimoFrameJogo = millis();

  switch (modoAtual) {
    case MODO_CLIMA:
      desenharTelaClima();
      break;
    case MODO_PACMAN:
      prepararPacman();
      break;
    case MODO_PITFALL:
      prepararPitfall();
      break;
    case MODO_ENDURO:
      prepararEnduro();
      break;
    case MODO_RIVER:
      prepararRiver();
      break;
  }
}

void atualizarJogo() {
  if (modoAtual == MODO_CLIMA) return;

  unsigned long agora = millis();
  if (agora - ultimoFrameJogo < GAME_FRAME_INTERVAL) return;
  ultimoFrameJogo = agora;
  frameJogo++;

  switch (modoAtual) {
    case MODO_PACMAN:
      atualizarPacman();
      break;
    case MODO_PITFALL:
      atualizarPitfall();
      break;
    case MODO_ENDURO:
      atualizarEnduro();
      break;
    case MODO_RIVER:
      atualizarRiver();
      break;
    default:
      break;
  }
}

// ============================================================
// FLASH 5 s: configuracao Wi-Fi
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
// Setup / Loop
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
    delay(1200);
  }
  carregarConfig();

  // Inicia Wi-Fi sem bloquear a troca de jogos via RST.
  if (!iniciarWifiSemBloquear()) {
    telaErro("WIFI", "Falha na configuracao");
    delay(2000);
  }

  prepararModoAtual();
}

void loop() {
  verificarBotaoConfig();
  manterWifi();

  unsigned long agora = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!primeiraConsultaFeita || agora - ultimaConsulta >= WEATHER_INTERVAL) {
      buscarClima();
    }
  }

  atualizarJogo();
  yield();
}

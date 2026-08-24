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
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long SCREEN_INTERVAL = 20UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const uint8_t TOTAL_SCREENS = 4;

String cidade = "Manaus";
String uf = "AM";
float latitude = -3.1190f;
float longitude = -60.0217f;
bool coordenadasValidas = true;

// Clima
float temperatura = NAN;
float sensacao = NAN;
float umidade = NAN;
float vento = NAN;
float rajada = NAN;
float pressao = NAN;
float precipitacao = NAN;
float nuvens = NAN;
float visibilidade = NAN;
int weatherCode = -1;
bool isDay = true;
String weatherTime = "";

// Qualidade do ar / poluentes
float aqi = NAN;
float pm25 = NAN;
float pm10 = NAN;
float co = NAN;
float no2 = NAN;
float so2 = NAN;
float ozonio = NAN;
float aerosol = NAN;
float poeira = NAN;
float uv = NAN;
String airTime = "";

unsigned long ultimaConsulta = 0;
unsigned long ultimaTrocaTela = 0;
uint8_t telaAtual = 0;

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

void rodapeAtualizado(const String& hora) {
  tft.setTextColor(tft.color565(120, 120, 120));
  tft.setTextSize(1);
  tft.setCursor(6, 119);
  tft.print("Atualizado ");
  tft.print(hora.length() ? hora : "--:--");
}

String horaISO(const String& iso) {
  if (iso.length() >= 16) return iso.substring(11, 16);
  return "--:--";
}

void valorLinha(int y, const String& rotulo, float valor, const String& unidade,
                uint16_t cor = ST77XX_WHITE, uint8_t casas = 0) {
  tft.setTextColor(cor);
  tft.setTextSize(1);
  tft.setCursor(6, y);
  tft.print(rotulo);
  if (isnan(valor)) {
    tft.print("--");
  } else {
    tft.print(valor, casas);
    tft.print(unidade);
  }
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

  if (!http.begin(client, url)) return false;

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
  if (erro) return false;

  JsonArray resultados = doc["results"].as<JsonArray>();
  if (resultados.isNull() || resultados.size() == 0) return false;

  JsonObject local = resultados[0];
  latitude = local["latitude"].as<float>();
  longitude = local["longitude"].as<float>();

  const char* nomeEncontrado = local["name"] | nullptr;
  if (nomeEncontrado && strlen(nomeEncontrado) > 0) cidade = nomeEncontrado;

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
  wm.setConfigPortalTimeout(0);
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
  if (!ok || WiFi.status() != WL_CONNECTED) return false;

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
  return WiFi.SSID().length() > 0;
}

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

  return WiFi.status() == WL_CONNECTED;
}

// ============================================================
// Clima e qualidade do ar - Open-Meteo
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

String descricaoAQI(float valor) {
  if (isnan(valor)) return "Sem dados";
  if (valor <= 50) return "Boa";
  if (valor <= 100) return "Moderada";
  if (valor <= 150) return "Ruim p/ sensiveis";
  if (valor <= 200) return "Ruim";
  if (valor <= 300) return "Muito ruim";
  return "Perigosa";
}

uint16_t corAQI(float valor) {
  if (isnan(valor)) return ST77XX_WHITE;
  if (valor <= 50) return ST77XX_GREEN;
  if (valor <= 100) return ST77XX_YELLOW;
  if (valor <= 150) return tft.color565(255, 140, 0);
  return ST77XX_RED;
}

String nivelFumaca() {
  if (isnan(pm25) && isnan(aerosol)) return "Sem dados";
  if ((!isnan(pm25) && pm25 >= 55.0f) || (!isnan(aerosol) && aerosol >= 1.0f)) return "ALTA";
  if ((!isnan(pm25) && pm25 >= 35.0f) || (!isnan(aerosol) && aerosol >= 0.5f)) return "MODERADA";
  return "BAIXA";
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

  valorLinha(96, "Sensacao: ", sensacao, "C");
  valorLinha(108, "Umidade:  ", umidade, "%");

  tft.setCursor(92, 96);
  tft.print("Vento:");
  tft.setCursor(92, 108);
  if (!isnan(vento)) tft.print(String(vento, 0) + " km/h"); else tft.print("--");

  rodapeAtualizado(horaISO(weatherTime));
}

void mostrarAtmosfera() {
  cabecalho("ATMOSFERA", ST77XX_CYAN);

  valorLinha(35, "Umidade       ", umidade, "%");
  valorLinha(50, "Pressao       ", pressao, " hPa");
  valorLinha(65, "Nuvens        ", nuvens, "%");
  valorLinha(80, "Visibilidade  ", isnan(visibilidade) ? NAN : visibilidade / 1000.0f, " km", ST77XX_WHITE, 1);
  valorLinha(95, "Precipitacao  ", precipitacao, " mm", ST77XX_WHITE, 1);
  valorLinha(110, "Rajadas       ", rajada, " km/h");

  rodapeAtualizado(horaISO(weatherTime));
}

void mostrarQualidadeAr() {
  cabecalho("QUALIDADE AR", corAQI(aqi));

  tft.setTextColor(corAQI(aqi));
  tft.setTextSize(2);
  tft.setCursor(6, 34);
  tft.print("AQI ");
  if (!isnan(aqi)) tft.print(aqi, 0); else tft.print("--");

  tft.setTextSize(1);
  tft.setCursor(86, 38);
  tft.print(descricaoAQI(aqi));

  valorLinha(62, "PM2.5   ", pm25, " ug/m3", ST77XX_WHITE, 1);
  valorLinha(77, "PM10    ", pm10, " ug/m3", ST77XX_WHITE, 1);
  valorLinha(92, "Ozonio  ", ozonio, " ug/m3", ST77XX_WHITE, 1);
  valorLinha(107, "UV      ", uv, "", ST77XX_WHITE, 1);

  rodapeAtualizado(horaISO(airTime));
}

void mostrarPoluentes() {
  cabecalho("FUMACA / AR", ST77XX_YELLOW);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 34);
  tft.print("Indicador fumaca: ");
  String fuma = nivelFumaca();
  if (fuma == "ALTA") tft.setTextColor(ST77XX_RED);
  else if (fuma == "MODERADA") tft.setTextColor(ST77XX_YELLOW);
  else tft.setTextColor(ST77XX_GREEN);
  tft.print(fuma);

  valorLinha(52, "CO       ", co, " ug/m3", ST77XX_WHITE, 0);
  valorLinha(66, "NO2      ", no2, " ug/m3", ST77XX_WHITE, 1);
  valorLinha(80, "SO2      ", so2, " ug/m3", ST77XX_WHITE, 1);
  valorLinha(94, "Aerosol  ", aerosol, "", ST77XX_WHITE, 2);
  valorLinha(108, "Poeira   ", poeira, " ug/m3", ST77XX_WHITE, 1);

  rodapeAtualizado(horaISO(airTime));
}

void mostrarTelaAtual() {
  switch (telaAtual) {
    case 0: mostrarClima(); break;
    case 1: mostrarAtmosfera(); break;
    case 2: mostrarQualidadeAr(); break;
    case 3: mostrarPoluentes(); break;
    default:
      telaAtual = 0;
      mostrarClima();
      break;
  }
}

void proximaTela() {
  telaAtual = (telaAtual + 1) % TOTAL_SCREENS;
  ultimaTrocaTela = millis();
  mostrarTelaAtual();
}

bool buscarClima() {
  if (WiFi.status() != WL_CONNECTED || !coordenadasValidas) return false;

  cabecalho("CLIMA");
  textoLinha(42, "Atualizando previsao...");

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,is_day,wind_speed_10m,wind_gusts_10m,surface_pressure,precipitation,cloud_cover,visibility" +
               "&timezone=auto";

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) return false;
  http.setTimeout(12000);

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Weather HTTP: ");
    Serial.println(codigo);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(6144);
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.print("Weather JSON: ");
    Serial.println(erro.c_str());
    return false;
  }

  JsonObject atual = doc["current"];
  temperatura = atual["temperature_2m"] | NAN;
  sensacao = atual["apparent_temperature"] | NAN;
  umidade = atual["relative_humidity_2m"] | NAN;
  vento = atual["wind_speed_10m"] | NAN;
  rajada = atual["wind_gusts_10m"] | NAN;
  pressao = atual["surface_pressure"] | NAN;
  precipitacao = atual["precipitation"] | NAN;
  nuvens = atual["cloud_cover"] | NAN;
  visibilidade = atual["visibility"] | NAN;
  weatherCode = atual["weather_code"] | -1;
  isDay = (atual["is_day"] | 1) == 1;
  weatherTime = String((const char*)(atual["time"] | ""));

  Serial.print("Clima atualizado: ");
  Serial.print(temperatura);
  Serial.print(" C / umidade ");
  Serial.print(umidade);
  Serial.println("%");
  return true;
}

bool buscarQualidadeAr() {
  if (WiFi.status() != WL_CONNECTED || !coordenadasValidas) return false;

  String url = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(latitude, 5) +
               "&longitude=" + String(longitude, 5) +
               "&current=us_aqi,pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,aerosol_optical_depth,dust,uv_index" +
               "&timezone=auto";

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) return false;
  http.setTimeout(12000);

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Air Quality HTTP: ");
    Serial.println(codigo);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(6144);
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.print("Air Quality JSON: ");
    Serial.println(erro.c_str());
    return false;
  }

  JsonObject atual = doc["current"];
  aqi = atual["us_aqi"] | NAN;
  pm10 = atual["pm10"] | NAN;
  pm25 = atual["pm2_5"] | NAN;
  co = atual["carbon_monoxide"] | NAN;
  no2 = atual["nitrogen_dioxide"] | NAN;
  so2 = atual["sulphur_dioxide"] | NAN;
  ozonio = atual["ozone"] | NAN;
  aerosol = atual["aerosol_optical_depth"] | NAN;
  poeira = atual["dust"] | NAN;
  uv = atual["uv_index"] | NAN;
  airTime = String((const char*)(atual["time"] | ""));

  Serial.print("AQI atualizado: ");
  Serial.print(aqi);
  Serial.print(" / PM2.5 ");
  Serial.println(pm25);
  return true;
}

void atualizarDados() {
  if (WiFi.status() != WL_CONNECTED) return;

  cabecalho("CLIMABOX");
  textoLinha(42, "Atualizando clima...");
  bool climaOk = buscarClima();

  textoLinha(62, "Atualizando qualidade ar...");
  bool arOk = buscarQualidadeAr();

  Serial.print("Atualizacao: clima=");
  Serial.print(climaOk ? "OK" : "ERRO");
  Serial.print(" ar=");
  Serial.println(arOk ? "OK" : "ERRO");

  mostrarTelaAtual();
  ultimaTrocaTela = millis();
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
      telaAtual = 0;
      atualizarDados();
      return;
    }

    delay(50);
    yield();
  }

  if (exibiuAviso) {
    mostrarTelaAtual();
    ultimaTrocaTela = millis();
  }
}

// ============================================================
// Setup / Loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(CONFIG_BUTTON, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
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

  telaAtual = 0;
  atualizarDados();
  ultimaConsulta = millis();
  ultimaTrocaTela = millis();
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
    if (WiFi.status() == WL_CONNECTED) atualizarDados();
  }

  if (agora - ultimaTrocaTela >= SCREEN_INTERVAL) {
    proximaTela();
  }

  delay(20);
}

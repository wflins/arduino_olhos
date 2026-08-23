#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// ESP32 WROOM-32 / DevKit
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000

// Wi-Fi: preencha com sua rede
const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";

// Localizacao padrao: Manaus-AM
// Troque latitude/longitude se quiser usar outra cidade.
const float LATITUDE = -3.1190;
const float LONGITUDE = -60.0217;

const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL; // 10 min
const unsigned long FRAME_INTERVAL = 120;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

float temperatura = NAN;
int weatherCode = -1;
bool isDay = true;
bool dadosValidos = false;
unsigned long ultimaConsulta = 0;
unsigned long ultimoFrame = 0;
uint8_t frameAnim = 0;

String descricaoTempo(int code, bool dia) {
  if (!dia && (code == 0 || code == 1 || code == 2 || code == 3)) return "NOITE";
  if (code == 0) return "SOL";
  if (code == 1 || code == 2) return "PARCIAL";
  if (code == 3) return "NUBLADO";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "CHUVA";
  if (code >= 95 && code <= 99) return "TEMPEST";
  if (code == 45 || code == 48) return "NEBLINA";
  return "CLIMA";
}

void desenharWifi(int x, int y) {
  if (WiFi.status() != WL_CONNECTED) {
    display.drawLine(x, y, x + 8, y + 8, SSD1306_WHITE);
    display.drawLine(x + 8, y, x, y + 8, SSD1306_WHITE);
    return;
  }
  display.drawCircle(x + 4, y + 7, 1, SSD1306_WHITE);
  display.drawArc(x + 4, y + 7, 3, 3, 200, 340, SSD1306_WHITE);
}

void desenharSol(int cx, int cy) {
  int pulso = (frameAnim % 4 < 2) ? 0 : 1;
  display.drawCircle(cx, cy, 8 + pulso, SSD1306_WHITE);
  display.fillCircle(cx, cy, 5, SSD1306_WHITE);
  for (int a = 0; a < 8; a++) {
    float ang = a * PI / 4.0;
    int x1 = cx + cos(ang) * 11;
    int y1 = cy + sin(ang) * 11;
    int x2 = cx + cos(ang) * 15;
    int y2 = cy + sin(ang) * 15;
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

void desenharLua(int cx, int cy) {
  display.fillCircle(cx, cy, 10, SSD1306_WHITE);
  display.fillCircle(cx + 5, cy - 3, 9, SSD1306_BLACK);
  display.drawPixel(cx - 13, cy - 8, SSD1306_WHITE);
  display.drawPixel(cx + 14, cy + 6, SSD1306_WHITE);
  display.drawPixel(cx - 8, cy + 13, SSD1306_WHITE);
}

void desenharNuvem(int x, int y) {
  int dx = (frameAnim % 6 < 3) ? 0 : 1;
  x += dx;
  display.fillCircle(x + 10, y + 8, 7, SSD1306_WHITE);
  display.fillCircle(x + 20, y + 6, 9, SSD1306_WHITE);
  display.fillCircle(x + 30, y + 9, 7, SSD1306_WHITE);
  display.fillRect(x + 8, y + 8, 24, 10, SSD1306_WHITE);
}

void desenharChuva(int x, int y) {
  desenharNuvem(x, y);
  for (int i = 0; i < 5; i++) {
    int yy = y + 22 + ((frameAnim * 3 + i * 5) % 13);
    int xx = x + 8 + i * 6;
    display.drawLine(xx, yy, xx - 2, yy + 5, SSD1306_WHITE);
  }
}

void desenharTempestade(int x, int y) {
  desenharChuva(x, y);
  if ((frameAnim % 8) < 3) {
    display.drawLine(x + 22, y + 20, x + 17, y + 31, SSD1306_WHITE);
    display.drawLine(x + 17, y + 31, x + 23, y + 31, SSD1306_WHITE);
    display.drawLine(x + 23, y + 31, x + 18, y + 42, SSD1306_WHITE);
  }
}

void desenharNeblina(int x, int y) {
  desenharNuvem(x, y);
  for (int i = 0; i < 3; i++) {
    int off = (frameAnim + i * 3) % 8;
    display.drawFastHLine(x + 2 + off, y + 24 + i * 6, 30 - off, SSD1306_WHITE);
  }
}

void desenharIconeTempo() {
  if (!dadosValidos) {
    display.setTextSize(1);
    display.setCursor(8, 26);
    display.print("SEM DADOS");
    return;
  }

  int code = weatherCode;
  if (!isDay && (code == 0 || code == 1 || code == 2 || code == 3)) {
    desenharLua(28, 29);
    if (code >= 2) desenharNuvem(28, 30);
  } else if (code == 0) {
    desenharSol(28, 29);
  } else if (code == 1 || code == 2) {
    desenharSol(20, 24);
    desenharNuvem(20, 26);
  } else if (code == 3) {
    desenharNuvem(8, 20);
  } else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    desenharChuva(8, 16);
  } else if (code >= 95 && code <= 99) {
    desenharTempestade(8, 14);
  } else if (code == 45 || code == 48) {
    desenharNeblina(8, 16);
  } else {
    desenharNuvem(8, 20);
  }
}

void desenharTela() {
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("CLIMA ATUAL");
  desenharWifi(116, 0);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  desenharIconeTempo();

  display.setTextSize(2);
  display.setCursor(61, 20);
  if (dadosValidos) {
    display.print(temperatura, 1);
    display.print("C");
  } else {
    display.print("--.-C");
  }

  display.setTextSize(1);
  display.setCursor(62, 42);
  display.print(descricaoTempo(weatherCode, isDay));

  display.setCursor(62, 54);
  display.print(isDay ? "DIA" : "NOITE");

  display.display();
}

bool conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 8);
  display.println("Conectando Wi-Fi...");
  display.setCursor(0, 24);
  display.println(WIFI_SSID);
  display.display();

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool buscarClima() {
  if (!conectarWifi()) {
    Serial.println("Falha ao conectar no Wi-Fi");
    return false;
  }

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current=temperature_2m,weather_code,is_day&timezone=auto";

  Serial.println(url);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP erro: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.print("Erro JSON: ");
    Serial.println(erro.c_str());
    return false;
  }

  temperatura = doc["current"]["temperature_2m"].as<float>();
  weatherCode = doc["current"]["weather_code"].as<int>();
  isDay = doc["current"]["is_day"].as<int>() == 1;
  dadosValidos = true;

  Serial.print("Temp: ");
  Serial.print(temperatura);
  Serial.print(" C | code: ");
  Serial.print(weatherCode);
  Serial.print(" | ");
  Serial.println(isDay ? "dia" : "noite");

  return true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println("SSD1306 nao encontrado");
    for (;;) delay(1000);
  }

  display.clearDisplay();
  display.display();

  buscarClima();
  ultimaConsulta = millis();
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimaConsulta >= WEATHER_INTERVAL) {
    ultimaConsulta = agora;
    buscarClima();
  }

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    frameAnim++;
    desenharTela();
  }
}

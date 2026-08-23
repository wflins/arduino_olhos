#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000

const char* WIFI_SSID = "SEU_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA";

// Manaus-AM = UTC-4, sem horario de verao
const long GMT_OFFSET_SEC = -4 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.nist.gov";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimoFrame = 0;
const unsigned long FRAME_INTERVAL = 250;

const char* diasSemana[] = {
  "DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"
};

void desenharWifi(int x, int y) {
  if (WiFi.status() != WL_CONNECTED) {
    display.drawLine(x, y, x + 8, y + 8, SSD1306_WHITE);
    display.drawLine(x + 8, y, x, y + 8, SSD1306_WHITE);
    return;
  }

  int rssi = WiFi.RSSI();
  int barras = 1;
  if (rssi > -80) barras = 2;
  if (rssi > -67) barras = 3;
  if (rssi > -55) barras = 4;

  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;
    if (i < barras) {
      display.fillRect(x + i * 3, y + 8 - h, 2, h, SSD1306_WHITE);
    } else {
      display.drawRect(x + i * 3, y + 8 - h, 2, h, SSD1306_WHITE);
    }
  }
}

bool conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(6, 18);
  display.println("CONECTANDO WIFI");
  display.setCursor(6, 34);
  display.println(WIFI_SSID);
  display.display();

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool sincronizarHora() {
  if (!conectarWifi()) return false;

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      return true;
    }
    delay(250);
  }

  return false;
}

void desenharRelogio() {
  struct tm t;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!getLocalTime(&t, 20)) {
    display.setTextSize(1);
    display.setCursor(18, 25);
    display.println("SEM HORA NTP");
    display.setCursor(12, 39);
    display.println("VERIFIQUE O WIFI");
    display.display();
    return;
  }

  char hh[3];
  char mm[3];
  char ss[3];
  snprintf(hh, sizeof(hh), "%02d", t.tm_hour);
  snprintf(mm, sizeof(mm), "%02d", t.tm_min);
  snprintf(ss, sizeof(ss), "%02d", t.tm_sec);

  // Cabecalho
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(diasSemana[t.tm_wday]);
  desenharWifi(116, 0);
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  // Hora grande
  display.setTextSize(3);
  display.setCursor(4, 17);
  display.print(hh);

  // Dois pontos piscando a cada segundo
  if ((t.tm_sec % 2) == 0) {
    display.fillCircle(45, 26, 2, SSD1306_WHITE);
    display.fillCircle(45, 37, 2, SSD1306_WHITE);
  }

  display.setCursor(52, 17);
  display.print(mm);

  // Segundos menores
  display.setTextSize(1);
  display.setCursor(103, 22);
  display.print(ss);

  // Data
  char data[16];
  snprintf(data, sizeof(data), "%02d/%02d/%04d",
           t.tm_mday,
           t.tm_mon + 1,
           t.tm_year + 1900);

  display.drawFastHLine(0, 49, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(31, 54);
  display.print(data);

  // Barra de progresso do minuto
  int largura = map(t.tm_sec, 0, 59, 0, SCREEN_WIDTH);
  display.drawFastHLine(0, 63, largura, SSD1306_WHITE);

  display.display();
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

  if (sincronizarHora()) {
    Serial.println("Hora sincronizada por NTP");
  } else {
    Serial.println("Falha ao sincronizar hora");
  }
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    desenharRelogio();
  }

  // Se o Wi-Fi cair, tenta reconectar periodicamente.
  static unsigned long ultimaTentativaWifi = 0;
  if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWifi >= 30000) {
    ultimaTentativaWifi = agora;
    conectarWifi();
  }
}

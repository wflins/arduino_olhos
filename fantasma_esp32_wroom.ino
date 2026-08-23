#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_random.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000
#define FRAME_INTERVAL 50

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimoFrame = 0;
unsigned long proximoBoo = 0;
unsigned long fimBoo = 0;
bool mostrarBoo = false;
int fase = 0;

void desenharFantasma(int cx, int cy) {
  int flutua = (int)(sin(fase * 0.16) * 2.0);
  int y = cy + flutua;

  // Cabeca e corpo
  display.fillCircle(cx, y - 10, 18, SSD1306_WHITE);
  display.fillRect(cx - 18, y - 10, 36, 28, SSD1306_WHITE);

  // Base ondulada
  display.fillTriangle(cx - 18, y + 18, cx - 10, y + 10, cx - 4, y + 18, SSD1306_WHITE);
  display.fillTriangle(cx - 6, y + 18, cx, y + 10, cx + 6, y + 18, SSD1306_WHITE);
  display.fillTriangle(cx + 4, y + 18, cx + 10, y + 10, cx + 18, y + 18, SSD1306_WHITE);

  // Olhos que observam lados diferentes
  int olhar = ((fase / 25) % 3) - 1;
  display.fillCircle(cx - 7, y - 11, 4, SSD1306_BLACK);
  display.fillCircle(cx + 7, y - 11, 4, SSD1306_BLACK);
  display.drawPixel(cx - 7 + olhar, y - 11, SSD1306_WHITE);
  display.drawPixel(cx + 7 + olhar, y - 11, SSD1306_WHITE);

  // Boca alterna expressao
  if ((fase / 18) % 2 == 0) {
    display.fillCircle(cx, y + 1, 3, SSD1306_BLACK);
  } else {
    display.drawFastHLine(cx - 4, y + 2, 8, SSD1306_BLACK);
  }

  // Bracos balancando
  int a = (fase / 6) % 8;
  int dy = (a < 4) ? a : 7 - a;
  display.drawLine(cx - 17, y, cx - 26, y - 5 + dy, SSD1306_WHITE);
  display.drawLine(cx + 17, y, cx + 26, y - 2 - dy, SSD1306_WHITE);
}

void desenharBoo() {
  if (!mostrarBoo) return;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 4);
  display.print(F("BOO!"));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) delay(1000);
  }
  randomSeed(esp_random());
  proximoBoo = millis() + random(1800, 4000);
  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long agora = millis();
  if (!mostrarBoo && (long)(agora - proximoBoo) >= 0) {
    mostrarBoo = true;
    fimBoo = agora + 700;
  }
  if (mostrarBoo && (long)(agora - fimBoo) >= 0) {
    mostrarBoo = false;
    proximoBoo = agora + random(2000, 5000);
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;
  fase++;

  display.clearDisplay();
  // Estrelinhas / poeira paranormal
  for (int i = 0; i < 8; i++) {
    int x = (i * 17 + fase * (i % 2 + 1)) % SCREEN_WIDTH;
    int y = (i * 11 + fase / 3) % SCREEN_HEIGHT;
    display.drawPixel(x, y, SSD1306_WHITE);
  }
  desenharFantasma(64, 31);
  desenharBoo();
  display.display();
}

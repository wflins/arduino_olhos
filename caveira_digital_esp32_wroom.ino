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
#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimoFrame = 0;
unsigned long proximoGlitch = 0;
unsigned long fimGlitch = 0;
bool glitchAtivo = false;
int fase = 0;

void desenharCaveira(int dx, int dy) {
  // Cranio
  display.drawRoundRect(37 + dx, 5 + dy, 54, 43, 14, SSD1306_WHITE);
  display.drawLine(43 + dx, 42 + dy, 43 + dx, 52 + dy, SSD1306_WHITE);
  display.drawLine(85 + dx, 42 + dy, 85 + dx, 52 + dy, SSD1306_WHITE);
  display.drawLine(43 + dx, 52 + dy, 85 + dx, 52 + dy, SSD1306_WHITE);

  // Macas do rosto
  display.drawLine(38 + dx, 30 + dy, 45 + dx, 39 + dy, SSD1306_WHITE);
  display.drawLine(90 + dx, 30 + dy, 83 + dx, 39 + dy, SSD1306_WHITE);

  // Olhos triangulares pulsando
  int olho = ((fase / 5) % 2) ? 1 : 0;
  display.fillTriangle(47 + dx, 21 + dy, 59 + dx, 18 + dy, 57 + dx, 31 + dy, SSD1306_WHITE);
  display.fillTriangle(81 + dx, 21 + dy, 69 + dx, 18 + dy, 71 + dx, 31 + dy, SSD1306_WHITE);
  if (olho) {
    display.fillCircle(55 + dx, 23 + dy, 2, SSD1306_BLACK);
    display.fillCircle(73 + dx, 23 + dy, 2, SSD1306_BLACK);
  }

  // Nariz
  display.fillTriangle(64 + dx, 29 + dy, 59 + dx, 37 + dy, 69 + dx, 37 + dy, SSD1306_WHITE);
  display.fillTriangle(64 + dx, 31 + dy, 62 + dx, 36 + dy, 66 + dx, 36 + dy, SSD1306_BLACK);

  // Mandibula / dentes
  int mordida = ((fase / 4) % 2) ? 1 : 0;
  int yBoca = 43 + dy + mordida;
  display.drawRect(50 + dx, yBoca, 28, 13, SSD1306_WHITE);
  for (int x = 54; x <= 74; x += 5) {
    display.drawFastVLine(x + dx, yBoca, 13, SSD1306_WHITE);
  }
  display.drawFastHLine(50 + dx, yBoca + 6, 28, SSD1306_WHITE);
}

void desenharRuido() {
  if (!glitchAtivo) return;
  for (int i = 0; i < 7; i++) {
    int y = random(0, SCREEN_HEIGHT);
    int x = random(0, 90);
    int w = random(8, 36);
    display.drawFastHLine(x, y, w, SSD1306_WHITE);
  }
  for (int i = 0; i < 12; i++) {
    display.drawPixel(random(0, SCREEN_WIDTH), random(0, SCREEN_HEIGHT), SSD1306_WHITE);
  }
}

void atualizarGlitch(unsigned long agora) {
  if (!glitchAtivo && (long)(agora - proximoGlitch) >= 0) {
    glitchAtivo = true;
    fimGlitch = agora + random(90, 260);
  }
  if (glitchAtivo && (long)(agora - fimGlitch) >= 0) {
    glitchAtivo = false;
    proximoGlitch = agora + random(900, 2600);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) delay(1000);
  }
  randomSeed(esp_random());
  proximoGlitch = millis() + 800;
  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long agora = millis();
  atualizarGlitch(agora);
  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;
  fase++;

  display.clearDisplay();
  int dx = glitchAtivo ? random(-2, 3) : 0;
  int dy = glitchAtivo ? random(-1, 2) : 0;
  desenharCaveira(dx, dy);
  desenharRuido();
  display.display();
}

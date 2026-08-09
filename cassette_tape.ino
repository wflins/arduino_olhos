#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define FRAME_INTERVAL 70

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

byte faseBobina = 0;
bool fitaFase = false;
byte vu1 = 2;
byte vu2 = 5;
unsigned long ultimoFrame = 0;
unsigned long ultimoVU = 0;

void desenharBobina(int cx, int cy, byte fase, bool inverter) {
  display.drawCircle(cx, cy, 9, SSD1306_WHITE);
  display.drawCircle(cx, cy, 5, SSD1306_WHITE);
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);

  byte f = inverter ? (4 - (fase % 4)) % 4 : fase % 4;

  if (f == 0) {
    display.drawLine(cx - 4, cy, cx + 4, cy, SSD1306_WHITE);
    display.drawLine(cx, cy - 4, cx, cy + 4, SSD1306_WHITE);
  }
  else if (f == 1) {
    display.drawLine(cx - 3, cy - 3, cx + 3, cy + 3, SSD1306_WHITE);
    display.drawLine(cx + 3, cy - 3, cx - 3, cy + 3, SSD1306_WHITE);
  }
  else if (f == 2) {
    display.drawLine(cx - 4, cy + 1, cx + 4, cy - 1, SSD1306_WHITE);
    display.drawLine(cx - 1, cy - 4, cx + 1, cy + 4, SSD1306_WHITE);
  }
  else {
    display.drawLine(cx - 3, cy + 3, cx + 3, cy - 3, SSD1306_WHITE);
    display.drawLine(cx - 3, cy - 3, cx + 3, cy + 3, SSD1306_WHITE);
  }
}

void desenharCassete() {
  // Corpo principal.
  display.drawRoundRect(5, 8, 118, 48, 5, SSD1306_WHITE);
  display.drawRoundRect(9, 12, 110, 40, 3, SSD1306_WHITE);

  // Janela central.
  display.drawRect(28, 18, 72, 22, SSD1306_WHITE);

  desenharBobina(45, 29, faseBobina, false);
  desenharBobina(83, 29, faseBobina, true);

  // Guia inferior da fita.
  display.drawLine(31, 43, 97, 43, SSD1306_WHITE);
  display.drawLine(36, 43, 43, 50, SSD1306_WHITE);
  display.drawLine(92, 43, 85, 50, SSD1306_WHITE);
  display.drawLine(43, 50, 85, 50, SSD1306_WHITE);

  // Movimento aparente da fita.
  for (int x = 46 + (fitaFase ? 2 : 0); x < 84; x += 6) {
    display.drawPixel(x, 48, SSD1306_WHITE);
  }

  // Parafusos.
  display.drawPixel(12, 15, SSD1306_WHITE);
  display.drawPixel(116, 15, SSD1306_WHITE);
  display.drawPixel(12, 49, SSD1306_WHITE);
  display.drawPixel(116, 49, SSD1306_WHITE);

  // Rotulo.
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(47, 10);
  display.print(F("SIDE A"));
}

void desenharVU() {
  // Pequeno medidor de nivel no centro.
  for (int i = 0; i < 8; i++) {
    if (i < vu1) {
      display.drawFastVLine(55 + i, 58 - i / 2, 3 + i / 2, SSD1306_WHITE);
    }

    if (i < vu2) {
      display.drawFastVLine(66 + i, 58 - i / 2, 3 + i / 2, SSD1306_WHITE);
    }
  }
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoVU >= 180) {
    ultimoVU = agora;
    vu1 = random(2, 8);
    vu2 = random(2, 8);
    fitaFase = !fitaFase;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  faseBobina++;

  display.clearDisplay();
  desenharCassete();
  desenharVU();
  display.display();
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  randomSeed((unsigned long)analogRead(A0) ^ micros());
  display.clearDisplay();
  display.display();
}

void loop() {
  atualizarAnimacao();
}

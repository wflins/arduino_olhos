#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000
#define FRAME_INTERVAL 55

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimoFrame = 0;
int pacX = 8;
int direcao = 1;
int fase = 0;

void desenharPacman(int x, int y, int dir) {
  bool bocaAberta = ((fase / 3) % 2) == 0;
  display.fillCircle(x, y, 7, SSD1306_WHITE);
  if (bocaAberta) {
    if (dir > 0) display.fillTriangle(x, y, x + 8, y - 6, x + 8, y + 6, SSD1306_BLACK);
    else display.fillTriangle(x, y, x - 8, y - 6, x - 8, y + 6, SSD1306_BLACK);
  }
}

void desenharFantasma(int x, int y, bool pernas) {
  display.fillCircle(x, y - 2, 6, SSD1306_WHITE);
  display.fillRect(x - 6, y - 2, 12, 8, SSD1306_WHITE);
  if (pernas) {
    display.fillTriangle(x - 6, y + 6, x - 3, y + 2, x, y + 6, SSD1306_WHITE);
    display.fillTriangle(x, y + 6, x + 3, y + 2, x + 6, y + 6, SSD1306_WHITE);
  }
  display.fillCircle(x - 2, y - 3, 2, SSD1306_BLACK);
  display.fillCircle(x + 3, y - 3, 2, SSD1306_BLACK);
  int olho = direcao > 0 ? 1 : -1;
  display.drawPixel(x - 2 + olho, y - 3, SSD1306_WHITE);
  display.drawPixel(x + 3 + olho, y - 3, SSD1306_WHITE);
}

void desenharCenario() {
  display.drawRect(0, 8, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.drawFastHLine(15, 19, 30, SSD1306_WHITE);
  display.drawFastHLine(83, 19, 30, SSD1306_WHITE);
  display.drawFastHLine(15, 44, 30, SSD1306_WHITE);
  display.drawFastHLine(83, 44, 30, SSD1306_WHITE);
  display.drawFastVLine(53, 8, 13, SSD1306_WHITE);
  display.drawFastVLine(74, 43, 13, SSD1306_WHITE);

  // Pellets no corredor principal
  for (int x = 7; x < 122; x += 10) {
    if (abs(x - pacX) > 8) display.fillCircle(x, 32, 1, SSD1306_WHITE);
  }
  display.fillCircle(12, 32, 2, SSD1306_WHITE);
  display.fillCircle(116, 32, 2, SSD1306_WHITE);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) delay(1000);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;
  fase++;

  pacX += direcao * 2;
  if (pacX >= 117) {
    pacX = 117;
    direcao = -1;
  } else if (pacX <= 10) {
    pacX = 10;
    direcao = 1;
  }

  display.clearDisplay();
  desenharCenario();
  desenharPacman(pacX, 32, direcao);

  int ghost1 = direcao > 0 ? pacX - 28 : pacX + 28;
  int ghost2 = direcao > 0 ? pacX - 48 : pacX + 48;
  if (ghost1 > 8 && ghost1 < 120) desenharFantasma(ghost1, 32, (fase / 3) % 2);
  if (ghost2 > 8 && ghost2 < 120) desenharFantasma(ghost2, 32, (fase / 4) % 2);

  display.display();
}

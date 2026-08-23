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
int roboX = -14;
int direcao = 1;
int fase = 0;

void desenharRobo(int x, int baseY, int dir) {
  int passo = (fase / 3) % 4;
  int pernaA = (passo < 2) ? 2 : -2;
  int pernaB = -pernaA;
  int bracoA = -pernaA;
  int bracoB = -pernaB;
  int bob = ((fase / 2) % 2);
  int y = baseY - bob;

  // Cabeca
  display.drawRoundRect(x - 7, y - 34, 14, 11, 2, SSD1306_WHITE);
  display.drawPixel(x - 3, y - 29, SSD1306_WHITE);
  display.drawPixel(x + 3, y - 29, SSD1306_WHITE);
  display.drawFastHLine(x - 2, y - 25, 5, SSD1306_WHITE);

  // Antena e luz piscando
  display.drawFastVLine(x, y - 39, 5, SSD1306_WHITE);
  if ((fase / 5) % 2 == 0) display.fillCircle(x, y - 40, 1, SSD1306_WHITE);

  // Corpo
  display.drawRect(x - 8, y - 22, 16, 15, SSD1306_WHITE);
  display.fillRect(x - 4, y - 18, 3, 3, SSD1306_WHITE);
  display.drawRect(x + 2, y - 18, 3, 3, SSD1306_WHITE);

  // Bracos alternados
  display.drawLine(x - 8, y - 19, x - 13, y - 11 + bracoA, SSD1306_WHITE);
  display.drawLine(x + 8, y - 19, x + 13, y - 11 + bracoB, SSD1306_WHITE);
  display.fillCircle(x - 13, y - 11 + bracoA, 1, SSD1306_WHITE);
  display.fillCircle(x + 13, y - 11 + bracoB, 1, SSD1306_WHITE);

  // Pernas andando
  display.drawLine(x - 4, y - 7, x - 5 + pernaA, y, SSD1306_WHITE);
  display.drawLine(x + 4, y - 7, x + 5 + pernaB, y, SSD1306_WHITE);
  display.drawFastHLine(x - 8 + pernaA, y, 5, SSD1306_WHITE);
  display.drawFastHLine(x + 3 + pernaB, y, 5, SSD1306_WHITE);

  // Pequena seta indica sentido quando ele vira
  if (dir > 0) display.drawPixel(x + 6, y - 30, SSD1306_WHITE);
  else display.drawPixel(x - 6, y - 30, SSD1306_WHITE);
}

void desenharChao() {
  display.drawFastHLine(0, 58, SCREEN_WIDTH, SSD1306_WHITE);
  for (int x = 0; x < SCREEN_WIDTH; x += 16) {
    int desloc = (fase * 2) % 16;
    int xx = direcao > 0 ? x - desloc : x + desloc;
    if (xx >= 0 && xx < SCREEN_WIDTH) display.drawFastHLine(xx, 61, 7, SSD1306_WHITE);
  }
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

  roboX += direcao;
  if (direcao > 0 && roboX > SCREEN_WIDTH + 14) {
    direcao = -1;
    roboX = SCREEN_WIDTH + 14;
  } else if (direcao < 0 && roboX < -14) {
    direcao = 1;
    roboX = -14;
  }

  display.clearDisplay();
  desenharChao();
  desenharRobo(roboX, 57, direcao);
  display.display();
}

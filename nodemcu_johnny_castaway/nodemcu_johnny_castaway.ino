/*
  Johnny Castaway inspired screensaver
  NodeMCU ESP8266 + TFT ST7735 128x160 SPI

  Bibliotecas:
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library

  Pinagem usada no sketch:
    CS    -> D8
    RESET -> D1
    A0/DC -> D2
    SCK   -> D5
    SDA   -> D7
    LED   -> 3V3
    VCC   -> 3V3
    GND   -> GND

  Se a pinagem fisica do seu modulo for diferente, ajuste apenas as defines abaixo.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   D8
#define TFT_RST  D1
#define TFT_DC   D2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Paleta
const uint16_t SKY_DAY   = 0x867D;
const uint16_t SKY_EVE   = 0xF3AA;
const uint16_t SKY_NIGHT = 0x18C7;
const uint16_t SEA       = 0x041F;
const uint16_t SEA_DARK  = 0x0316;
const uint16_t SAND      = 0xF5A6;
const uint16_t TRUNK     = 0x8A22;
const uint16_t LEAF      = 0x05E8;
const uint16_t SKIN      = 0xFD6A;
const uint16_t SHIRT     = 0x001F;
const uint16_t SHORTS    = 0x780F;
const uint16_t CLOUD     = 0xFFFF;

void pauseMs(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) delay(1);
}

void drawSea(uint16_t sky, bool night = false) {
  tft.fillScreen(sky);
  tft.fillRect(0, 92, 128, 68, night ? SEA_DARK : SEA);

  for (int y = 96; y < 158; y += 8) {
    int off = ((y / 8) % 2) * 7;
    for (int x = -10 + off; x < 128; x += 22) {
      tft.drawFastHLine(x, y, 10, night ? 0x4A69 : 0x9E7F);
    }
  }
}

void drawIsland() {
  tft.fillEllipse(64, 119, 52, 18, SAND);
  tft.fillEllipse(64, 126, 45, 12, 0xDCE3);
}

void drawPalm(int x = 32, int y = 112) {
  for (int i = 0; i < 5; i++) {
    tft.drawLine(x + i, y, x + 8 + i, y - 53, TRUNK);
  }

  int cx = x + 10;
  int cy = y - 55;

  tft.fillCircle(cx, cy, 4, 0x6240);
  tft.drawLine(cx, cy, cx - 25, cy - 8, LEAF);
  tft.drawLine(cx, cy, cx - 18, cy - 17, LEAF);
  tft.drawLine(cx, cy, cx + 25, cy - 8, LEAF);
  tft.drawLine(cx, cy, cx + 20, cy - 18, LEAF);
  tft.drawLine(cx, cy, cx + 5, cy - 25, LEAF);

  for (int i = 0; i < 3; i++) {
    tft.drawLine(cx - 24, cy - 8 + i, cx, cy + i, LEAF);
    tft.drawLine(cx + 24, cy - 8 + i, cx, cy + i, LEAF);
  }

  tft.fillCircle(cx - 3, cy + 4, 3, 0x7A20);
  tft.fillCircle(cx + 3, cy + 3, 3, 0x7A20);
}

void drawCloud(int x, int y) {
  tft.fillCircle(x, y + 4, 6, CLOUD);
  tft.fillCircle(x + 7, y, 8, CLOUD);
  tft.fillCircle(x + 15, y + 4, 6, CLOUD);
  tft.fillRect(x, y + 4, 16, 7, CLOUD);
}

void drawSun(int x, int y) {
  tft.fillCircle(x, y, 8, ST77XX_YELLOW);
  for (int a = 0; a < 360; a += 45) {
    float r = a * 3.14159 / 180.0;
    tft.drawLine(x + cos(r) * 11, y + sin(r) * 11,
                 x + cos(r) * 15, y + sin(r) * 15, ST77XX_YELLOW);
  }
}

void drawMoon(int x, int y) {
  tft.fillCircle(x, y, 8, 0xFFDE);
  tft.fillCircle(x + 4, y - 2, 7, SKY_NIGHT);
}

void drawStars() {
  const uint8_t stars[][2] = {
    {10, 18},{23, 30},{47, 15},{63, 34},{82, 12},{101, 29},{118, 18},{91, 47},{34, 49}
  };
  for (auto &s : stars) {
    tft.drawPixel(s[0], s[1], ST77XX_WHITE);
    tft.drawPixel(s[0] + 1, s[1], ST77XX_WHITE);
  }
}

void drawJohnny(int x, int y, int pose = 0) {
  // cabeca
  tft.fillCircle(x, y - 27, 6, SKIN);
  tft.drawPixel(x + 2, y - 29, ST77XX_BLACK);
  tft.drawPixel(x + 4, y - 26, ST77XX_BLACK);

  // cabelo
  tft.drawFastHLine(x - 4, y - 33, 8, ST77XX_BLACK);
  tft.drawPixel(x - 5, y - 31, ST77XX_BLACK);

  // corpo
  tft.fillRect(x - 5, y - 21, 10, 14, SHIRT);
  tft.fillRect(x - 5, y - 7, 10, 6, SHORTS);

  // pernas
  tft.drawLine(x - 2, y, x - 4, y + 10, SKIN);
  tft.drawLine(x + 2, y, x + 4, y + 10, SKIN);
  tft.drawFastHLine(x - 8, y + 10, 5, ST77XX_BLACK);
  tft.drawFastHLine(x + 3, y + 10, 5, ST77XX_BLACK);

  // bracos / poses
  if (pose == 1) { // acena
    tft.drawLine(x - 5, y - 18, x - 12, y - 28, SKIN);
    tft.drawLine(x + 5, y - 18, x + 10, y - 10, SKIN);
  } else if (pose == 2) { // pesca
    tft.drawLine(x - 5, y - 17, x - 12, y - 10, SKIN);
    tft.drawLine(x + 5, y - 17, x + 12, y - 9, SKIN);
  } else if (pose == 3) { // assustado
    tft.drawLine(x - 5, y - 18, x - 12, y - 30, SKIN);
    tft.drawLine(x + 5, y - 18, x + 12, y - 30, SKIN);
  } else {
    tft.drawLine(x - 5, y - 18, x - 9, y - 8, SKIN);
    tft.drawLine(x + 5, y - 18, x + 9, y - 8, SKIN);
  }
}

void drawBoat(int x, int y) {
  tft.fillTriangle(x, y, x + 24, y, x + 19, y + 7, 0x7A20);
  tft.drawLine(x + 12, y, x + 12, y - 18, ST77XX_BLACK);
  tft.fillTriangle(x + 13, y - 17, x + 13, y - 2, x + 24, y - 5, ST77XX_WHITE);
}

void sceneIdle() {
  drawSea(SKY_DAY);
  drawSun(105, 22);
  drawCloud(13, 24);
  drawIsland();
  drawPalm();
  drawJohnny(78, 121, 0);
  pauseMs(3000);
}

void sceneFishing() {
  drawSea(SKY_DAY);
  drawCloud(82, 25);
  drawIsland();
  drawPalm();
  drawJohnny(77, 121, 2);

  tft.drawLine(88, 112, 112, 101, 0xC618);
  tft.drawLine(112, 101, 116, 130, 0xC618);

  for (int i = 0; i < 5; i++) {
    tft.fillCircle(116, 130, 2, (i % 2) ? ST77XX_WHITE : ST77XX_RED);
    pauseMs(350);
  }
  pauseMs(1000);
}

void sceneBoat() {
  drawSea(SKY_DAY);
  drawIsland();
  drawPalm();
  drawJohnny(78, 121, 1);

  for (int x = 128; x > 64; x -= 3) {
    tft.fillRect(60, 67, 68, 28, SKY_DAY);
    drawBoat(x, 83);
    pauseMs(90);
  }
  pauseMs(800);
}

void sceneCoconut() {
  drawSea(SKY_DAY);
  drawIsland();
  drawPalm();
  drawJohnny(76, 121, 0);

  for (int y = 61; y < 103; y += 4) {
    drawSea(SKY_DAY);
    drawIsland();
    drawPalm();
    drawJohnny(76, 121, (y > 90) ? 3 : 0);
    tft.fillCircle(39, y, 4, 0x7A20);
    pauseMs(90);
  }

  tft.fillCircle(39, 106, 4, 0x7A20);
  pauseMs(1200);
}

void sceneRain() {
  drawSea(0x6B6D);
  drawIsland();
  drawPalm();
  drawJohnny(76, 121, 0);

  for (int f = 0; f < 20; f++) {
    for (int x = 5; x < 125; x += 13) {
      int y = (x * 7 + f * 9) % 90;
      tft.drawLine(x, y, x - 2, y + 5, 0xBDF7);
    }
    pauseMs(80);
    drawSea(0x6B6D);
    drawIsland();
    drawPalm();
    drawJohnny(76, 121, 0);
  }
}

void sceneNight() {
  drawSea(SKY_NIGHT, true);
  drawMoon(103, 20);
  drawStars();
  drawIsland();
  drawPalm();

  // Johnny deitado
  tft.fillCircle(72, 111, 5, SKIN);
  tft.fillRect(76, 108, 18, 7, SHIRT);
  tft.drawLine(94, 112, 103, 115, SKIN);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(82, 96);
  tft.print("Z");
  pauseMs(500);
  tft.setCursor(91, 88);
  tft.print("Z");
  pauseMs(500);
  tft.setCursor(101, 79);
  tft.print("Z");
  pauseMs(1800);
}

void sceneSOS() {
  drawSea(SKY_EVE);
  drawIsland();
  drawPalm();
  drawJohnny(80, 121, 0);

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(50, 100);
  tft.print("SOS");
  pauseMs(2200);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();

  // Se houver deslocamento/cores estranhas, teste GREENTAB ou REDTAB.
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(false);

  randomSeed(ESP.getCycleCount());

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(20, 62);
  tft.println("CASTAWAY");
  tft.setCursor(25, 76);
  tft.println("ESP8266");
  pauseMs(1200);
}

void loop() {
  // Ordem semi-aleatoria para dar sensacao de screensaver vivo.
  uint8_t scene = random(0, 7);

  switch (scene) {
    case 0: sceneIdle();    break;
    case 1: sceneFishing(); break;
    case 2: sceneBoat();    break;
    case 3: sceneCoconut(); break;
    case 4: sceneRain();    break;
    case 5: sceneNight();   break;
    case 6: sceneSOS();     break;
  }
}

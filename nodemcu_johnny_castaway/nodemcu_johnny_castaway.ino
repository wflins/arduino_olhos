/*
  Castaway screensaver - NodeMCU ESP8266 + TFT ST7735 128x160 SPI

  Versao com personagem maior e mais detalhado, inspirada na ideia de
  um screensaver classico de naufrago, mas com arte desenhada por primitivas.

  Bibliotecas:
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library

  Pinagem:
    CS    -> D8
    RESET -> D1
    A0/DC -> D2
    SCK   -> D5
    SDA   -> D7
    LED   -> 3V3
    VCC   -> 3V3
    GND   -> GND
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   D8
#define TFT_RST  D1
#define TFT_DC   D2

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Paleta RGB565
const uint16_t SKY_DAY   = 0x867D;
const uint16_t SKY_EVE   = 0xF3AA;
const uint16_t SKY_NIGHT = 0x18C7;
const uint16_t SEA       = 0x041F;
const uint16_t SEA_DARK  = 0x0316;
const uint16_t FOAM      = 0xDFFF;
const uint16_t SAND      = 0xF5A6;
const uint16_t SAND_DARK = 0xDCE3;
const uint16_t TRUNK     = 0x8A22;
const uint16_t LEAF      = 0x05E8;
const uint16_t SKIN      = 0xFD6A;
const uint16_t SKIN_DARK = 0xE448;
const uint16_t SHIRT     = 0x001F;
const uint16_t SHIRT_LIT = 0x4C9F;
const uint16_t SHORTS    = 0x780F;
const uint16_t HAIR      = 0x3100;
const uint16_t BEARD     = 0x5200;
const uint16_t CLOUD     = 0xFFFF;

void pauseMs(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) delay(1);
}

void drawSea(uint16_t sky, bool night = false, uint8_t phase = 0) {
  tft.fillScreen(sky);
  tft.fillRect(0, 88, 128, 72, night ? SEA_DARK : SEA);

  uint16_t wave = night ? 0x4A69 : 0x9E7F;
  for (int y = 94; y < 158; y += 8) {
    int off = ((y / 8) % 2) * 9 + phase;
    for (int x = -18 + off; x < 128; x += 24) {
      tft.drawFastHLine(x, y, 10, wave);
      tft.drawPixel(x + 10, y + 1, wave);
      tft.drawPixel(x + 11, y + 2, wave);
    }
  }
}

void drawIsland() {
  // sombra e massa principal
  tft.fillEllipse(65, 125, 58, 22, SAND_DARK);
  tft.fillEllipse(63, 119, 55, 19, SAND);

  // faixa de espuma na borda frontal
  tft.drawFastHLine(21, 135, 84, FOAM);
  tft.drawFastHLine(29, 137, 67, FOAM);

  // pequenos graos/pedras
  tft.fillCircle(51, 111, 1, SAND_DARK);
  tft.fillCircle(91, 116, 1, SAND_DARK);
  tft.fillCircle(36, 123, 1, SAND_DARK);
}

void drawPalm(int x = 22, int y = 118) {
  // tronco inclinado e mais grosso
  for (int i = 0; i < 6; i++) {
    tft.drawLine(x + i, y, x + 15 + i, y - 60, TRUNK);
  }

  // segmentos no tronco
  for (int s = 0; s < 5; s++) {
    int yy = y - s * 11;
    int xx = x + 2 + s * 3;
    tft.drawLine(xx, yy, xx + 6, yy - 2, 0x6A00);
  }

  int cx = x + 18;
  int cy = y - 63;
  tft.fillCircle(cx, cy, 5, 0x6240);

  // folhas com espessura de 2 pixels
  const int ends[][2] = {
    {-29,-7}, {-24,-18}, {-11,-27}, {5,-30},
    {24,-20}, {30,-8}, {25,5}, {-22,4}
  };

  for (auto &e : ends) {
    tft.drawLine(cx, cy, cx + e[0], cy + e[1], LEAF);
    tft.drawLine(cx, cy + 1, cx + e[0], cy + e[1] + 1, LEAF);
  }

  tft.fillCircle(cx - 4, cy + 5, 4, 0x7A20);
  tft.fillCircle(cx + 4, cy + 4, 4, 0x7A20);
}

void drawCloud(int x, int y) {
  tft.fillCircle(x, y + 4, 6, CLOUD);
  tft.fillCircle(x + 8, y, 8, CLOUD);
  tft.fillCircle(x + 17, y + 4, 7, CLOUD);
  tft.fillRect(x, y + 4, 18, 8, CLOUD);
}

void drawSun(int x, int y) {
  tft.fillCircle(x, y, 9, ST77XX_YELLOW);
  for (int a = 0; a < 360; a += 45) {
    float r = a * 3.14159 / 180.0;
    tft.drawLine(x + cos(r) * 12, y + sin(r) * 12,
                 x + cos(r) * 16, y + sin(r) * 16, ST77XX_YELLOW);
  }
}

void drawMoon(int x, int y) {
  tft.fillCircle(x, y, 9, 0xFFDE);
  tft.fillCircle(x + 4, y - 3, 8, SKY_NIGHT);
}

void drawStars() {
  const uint8_t stars[][2] = {
    {9,17},{22,31},{49,15},{65,34},{83,12},{101,29},{118,18},{91,47},{35,49}
  };
  for (auto &s : stars) {
    tft.drawPixel(s[0], s[1], ST77XX_WHITE);
    tft.drawPixel(s[0] + 1, s[1], ST77XX_WHITE);
  }
}

// pose:
// 0 parado
// 1 acenando
// 2 pescando
// 3 assustado
// 4 olhando horizonte
void drawJohnny(int x, int y, uint8_t pose = 0) {
  // y = nivel dos pes. Personagem ~48 px de altura.

  // pernas
  if (pose == 4) {
    tft.fillRect(x - 7, y - 14, 5, 15, SKIN_DARK);
    tft.fillRect(x + 2, y - 14, 5, 15, SKIN);
  } else {
    tft.fillTriangle(x - 7, y - 14, x - 1, y - 14, x - 6, y + 1, SKIN);
    tft.fillTriangle(x + 1, y - 14, x + 7, y - 14, x + 6, y + 1, SKIN);
  }

  // pes
  tft.fillRect(x - 10, y, 7, 3, 0x3186);
  tft.fillRect(x + 3, y, 7, 3, 0x3186);

  // shorts
  tft.fillRect(x - 9, y - 21, 18, 9, SHORTS);
  tft.drawFastVLine(x, y - 20, 7, 0x5008);

  // torso ligeiramente trapezoidal
  tft.fillTriangle(x - 10, y - 22, x + 10, y - 22, x - 7, y - 40, SHIRT);
  tft.fillTriangle(x + 10, y - 22, x + 7, y - 40, x - 7, y - 40, SHIRT);
  tft.drawFastHLine(x - 5, y - 38, 10, SHIRT_LIT);

  // pescoco
  tft.fillRect(x - 3, y - 44, 6, 5, SKIN_DARK);

  // cabeca maior
  tft.fillCircle(x, y - 51, 10, SKIN);
  tft.fillCircle(x - 1, y - 53, 8, SKIN);

  // orelha e nariz
  tft.fillCircle(x - 9, y - 51, 2, SKIN_DARK);
  tft.fillTriangle(x + 8, y - 53, x + 13, y - 50, x + 8, y - 48, SKIN);

  // cabelo desgrenhado
  tft.fillTriangle(x - 8, y - 59, x - 2, y - 65, x, y - 59, HAIR);
  tft.fillTriangle(x - 2, y - 61, x + 4, y - 66, x + 5, y - 59, HAIR);
  tft.fillTriangle(x + 3, y - 60, x + 10, y - 63, x + 8, y - 56, HAIR);

  // barba curta
  tft.fillTriangle(x - 5, y - 47, x + 7, y - 47, x + 2, y - 41, BEARD);

  // olho e sobrancelha
  tft.drawPixel(x + 4, y - 54, ST77XX_BLACK);
  tft.drawFastHLine(x + 2, y - 57, 5, HAIR);

  // boca
  tft.drawFastHLine(x + 2, y - 46, 4, ST77XX_BLACK);

  // bracos com ombros mais largos
  if (pose == 1) { // acenando
    tft.drawLine(x - 8, y - 36, x - 17, y - 48, SKIN);
    tft.drawLine(x - 17, y - 48, x - 14, y - 60, SKIN);
    tft.fillCircle(x - 14, y - 61, 2, SKIN);

    tft.drawLine(x + 8, y - 36, x + 14, y - 25, SKIN);
  }
  else if (pose == 2) { // pescando
    tft.drawLine(x - 8, y - 35, x - 15, y - 28, SKIN);
    tft.drawLine(x + 8, y - 35, x + 17, y - 28, SKIN);
    tft.fillCircle(x + 17, y - 28, 2, SKIN);
  }
  else if (pose == 3) { // assustado
    tft.drawLine(x - 8, y - 36, x - 17, y - 51, SKIN);
    tft.drawLine(x + 8, y - 36, x + 17, y - 51, SKIN);
    tft.fillCircle(x - 17, y - 52, 2, SKIN);
    tft.fillCircle(x + 17, y - 52, 2, SKIN);

    // boca aberta
    tft.fillCircle(x + 4, y - 46, 2, ST77XX_BLACK);
  }
  else if (pose == 4) { // mao na testa olhando longe
    tft.drawLine(x - 8, y - 36, x - 14, y - 25, SKIN);
    tft.drawLine(x + 8, y - 36, x + 13, y - 48, SKIN);
    tft.drawLine(x + 13, y - 48, x + 5, y - 55, SKIN);
  }
  else { // relaxado
    tft.drawLine(x - 8, y - 36, x - 13, y - 23, SKIN);
    tft.drawLine(x + 8, y - 36, x + 13, y - 23, SKIN);
  }
}

void drawBoat(int x, int y) {
  tft.fillTriangle(x, y, x + 24, y, x + 19, y + 7, 0x7A20);
  tft.drawLine(x + 12, y, x + 12, y - 18, ST77XX_BLACK);
  tft.fillTriangle(x + 13, y - 17, x + 13, y - 2, x + 24, y - 5, ST77XX_WHITE);
}

void sceneIdle() {
  for (uint8_t f = 0; f < 8; f++) {
    drawSea(SKY_DAY, false, f % 4);
    drawSun(105, 20);
    drawCloud(10, 25);
    drawIsland();
    drawPalm();
    drawJohnny(80, 131, (f > 4) ? 4 : 0);
    pauseMs(220);
  }
  pauseMs(900);
}

void sceneFishing() {
  drawSea(SKY_DAY);
  drawCloud(83, 23);
  drawIsland();
  drawPalm();
  drawJohnny(76, 131, 2);

  // vara e linha
  tft.drawLine(93, 103, 115, 91, 0xC618);
  tft.drawLine(115, 91, 119, 133, 0xC618);

  for (int i = 0; i < 7; i++) {
    tft.fillCircle(119, 133, 2, (i % 2) ? ST77XX_WHITE : ST77XX_RED);
    pauseMs(260);
  }
  pauseMs(700);
}

void sceneBoat() {
  for (int x = 126; x > 67; x -= 3) {
    drawSea(SKY_DAY, false, x % 4);
    drawIsland();
    drawPalm();
    drawJohnny(78, 131, 1);
    drawBoat(x, 78);
    pauseMs(85);
  }
  pauseMs(700);
}

void sceneCoconut() {
  for (int y = 61; y < 107; y += 4) {
    drawSea(SKY_DAY);
    drawIsland();
    drawPalm();
    drawJohnny(78, 131, (y > 92) ? 3 : 0);
    tft.fillCircle(40, y, 4, 0x7A20);
    pauseMs(90);
  }
  pauseMs(900);
}

void sceneRain() {
  for (int f = 0; f < 18; f++) {
    drawSea(0x6B6D, false, f % 4);
    drawIsland();
    drawPalm();
    drawJohnny(78, 131, (f % 3 == 0) ? 3 : 0);

    for (int x = 5; x < 125; x += 13) {
      int y = (x * 7 + f * 9) % 86;
      tft.drawLine(x, y, x - 2, y + 5, 0xBDF7);
    }
    pauseMs(95);
  }
}

void sceneNight() {
  drawSea(SKY_NIGHT, true);
  drawMoon(103, 20);
  drawStars();
  drawIsland();
  drawPalm();

  // Johnny deitado, proporcional ao novo personagem
  tft.fillCircle(68, 114, 8, SKIN);
  tft.fillRect(76, 110, 25, 10, SHIRT);
  tft.fillRect(99, 113, 15, 7, SHORTS);
  tft.drawLine(111, 118, 121, 121, SKIN);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(83, 96); tft.print("Z");
  pauseMs(450);
  tft.setCursor(93, 87); tft.print("Z");
  pauseMs(450);
  tft.setCursor(104, 77); tft.print("Z");
  pauseMs(1600);
}

void sceneSOS() {
  drawSea(SKY_EVE);
  drawIsland();
  drawPalm();
  drawJohnny(88, 131, 4);

  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(47, 105);
  tft.print("SOS");
  pauseMs(1900);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();

  // Se houver deslocamento/cores estranhas, teste INITR_GREENTAB ou INITR_REDTAB.
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setTextWrap(false);

  randomSeed(ESP.getCycleCount());

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 62);
  tft.println("CASTAWAY");
  tft.setCursor(28, 76);
  tft.println("ESP8266");
  pauseMs(1000);
}

void loop() {
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

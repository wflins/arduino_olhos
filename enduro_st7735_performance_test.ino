#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Teste focado em fluidez: sem Wi-Fi, sem HTTPS, sem LittleFS.
// Objetivo: medir o limite real de animacao da TFT/ESP8266.

const uint16_t FRAME_MS = 33; // alvo ~30 FPS
const int SCENE_Y = 26;
const int SCENE_H = 82;

GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0;
uint16_t fpsValue = 0;

// Sprite simples para os adversarios.
const uint8_t PROGMEM CAR_ENEMY[] = {
  0x03,0xC0,
  0x07,0xE0,
  0x0F,0xF0,
  0x1F,0xF8,
  0x3C,0x3C,
  0x7E,0x7E,
  0xFF,0xFF,
  0xDB,0xDB,
  0xC3,0xC3,
  0x81,0x81,
  0x81,0x81,
  0x00,0x00
};

inline int roadLeftAt(int y) {
  return 77 - (y * 58) / (SCENE_H - 1);
}

inline int roadRightAt(int y) {
  return 83 + (y * 58) / (SCENE_H - 1);
}

void drawRoadBase() {
  const uint16_t sky = tft.color565(78, 145, 215);
  const uint16_t grass = tft.color565(40, 135, 45);
  const uint16_t road = tft.color565(58, 58, 62);
  const uint16_t snow = tft.color565(236, 236, 236);

  framebuf.fillScreen(sky);
  framebuf.fillRect(0, 17, 160, SCENE_H - 17, grass);

  framebuf.fillTriangle(78, 17, 18, SCENE_H - 1, 80, SCENE_H - 1, road);
  framebuf.fillTriangle(82, 17, 80, SCENE_H - 1, 142, SCENE_H - 1, road);
  framebuf.fillRect(78, 17, 5, 3, road);

  framebuf.drawLine(78, 17, 18, SCENE_H - 1, snow);
  framebuf.drawLine(82, 17, 142, SCENE_H - 1, snow);
}

void drawLaneMarkers() {
  const uint16_t yellow = ST77XX_YELLOW;
  const int phase = (frameNo * 4) % 32;

  for (int y = 20 + phase; y < SCENE_H; y += 32) {
    int dy = y - 17;
    int w = 1 + dy / 18;
    int h = 3 + dy / 11;
    framebuf.fillRect(80 - w / 2, y, w, h, yellow);
  }
}

void drawRoadsidePosts() {
  const uint16_t white = ST77XX_WHITE;
  const uint16_t red = tft.color565(230, 45, 45);
  const int phase = (frameNo * 5) % 28;

  for (int y = 20 + phase; y < SCENE_H; y += 28) {
    int lx = roadLeftAt(y) - 5;
    int rx = roadRightAt(y) + 5;
    int h = 3 + (y - 17) / 9;
    framebuf.fillRect(lx, y - h, 2, h, white);
    framebuf.fillRect(rx, y - h, 2, h, white);
    if (((y / 28) & 1) == 0) {
      framebuf.drawPixel(lx, y - h, red);
      framebuf.drawPixel(rx, y - h, red);
    }
  }
}

void drawBitmapScaled1bit(int x, int y, const uint8_t* bmp, int w, int h,
                          uint16_t color, uint8_t scale) {
  for (int yy = 0; yy < h; yy++) {
    for (int xx = 0; xx < w; xx++) {
      uint16_t bitIndex = yy * w + xx;
      uint8_t b = pgm_read_byte(bmp + (bitIndex >> 3));
      if (b & (0x80 >> (bitIndex & 7))) {
        if (scale == 1) framebuf.drawPixel(x + xx, y + yy, color);
        else framebuf.fillRect(x + xx * scale, y + yy * scale, scale, scale, color);
      }
    }
  }
}

void drawEnemy() {
  const uint16_t colors[3] = {
    ST77XX_RED,
    ST77XX_CYAN,
    tft.color565(240, 125, 35)
  };

  uint16_t cycle = (frameNo * 3) % 145;
  int y = 20 + cycle / 2;
  if (y >= SCENE_H - 16) return;

  int lane = ((frameNo / 145) % 3) - 1;
  int spread = 3 + (y - 17) / 6;
  int x = 80 + lane * spread;
  uint8_t scale = (y > 54) ? 2 : 1;
  int sw = 16 * scale;
  int sh = 12 * scale;

  drawBitmapScaled1bit(x - sw / 2, y - sh / 2, CAR_ENEMY, 16, 12,
                       colors[(frameNo / 145) % 3], scale);
}

// Carro de Formula 1 visto de tras, desenhado em poucas primitivas.
// Mantem o custo baixo, mas fica bem mais reconhecivel que o sprite anterior.
void drawF1Player(int cx, int baseY) {
  const uint16_t body = tft.color565(225, 28, 28);
  const uint16_t bodyDark = tft.color565(150, 12, 12);
  const uint16_t black = ST77XX_BLACK;
  const uint16_t tire = tft.color565(20, 20, 20);
  const uint16_t tireHi = tft.color565(75, 75, 75);
  const uint16_t glass = tft.color565(85, 165, 225);
  const uint16_t white = ST77XX_WHITE;
  const uint16_t yellow = ST77XX_YELLOW;

  // Oscilacao minima da suspensao para dar vida sem tremer demais.
  int bob = ((frameNo / 5) & 1) ? 1 : 0;
  int y = baseY + bob;

  // Asa traseira larga.
  framebuf.fillRect(cx - 18, y - 7, 36, 4, black);
  framebuf.fillRect(cx - 15, y - 8, 30, 2, bodyDark);
  framebuf.fillRect(cx - 17, y - 3, 4, 3, black);
  framebuf.fillRect(cx + 13, y - 3, 4, 3, black);

  // Pneus traseiros grandes.
  framebuf.fillRoundRect(cx - 20, y - 2, 8, 16, 2, tire);
  framebuf.fillRoundRect(cx + 12, y - 2, 8, 16, 2, tire);
  framebuf.drawFastVLine(cx - 18, y + 1, 9, tireHi);
  framebuf.drawFastVLine(cx + 18, y + 1, 9, tireHi);

  // Corpo central e sidepods.
  framebuf.fillTriangle(cx, y - 20, cx - 10, y + 10, cx + 10, y + 10, body);
  framebuf.fillRect(cx - 12, y - 2, 24, 9, body);
  framebuf.fillTriangle(cx - 12, y - 1, cx - 18, y + 7, cx - 8, y + 7, bodyDark);
  framebuf.fillTriangle(cx + 12, y - 1, cx + 18, y + 7, cx + 8, y + 7, bodyDark);

  // Cockpit / halo simplificado.
  framebuf.fillTriangle(cx, y - 18, cx - 5, y - 7, cx + 5, y - 7, glass);
  framebuf.drawLine(cx - 6, y - 9, cx, y - 14, black);
  framebuf.drawLine(cx + 6, y - 9, cx, y - 14, black);
  framebuf.drawFastHLine(cx - 5, y - 9, 11, black);

  // Tampa do motor / espinha traseira.
  framebuf.fillRect(cx - 3, y - 7, 6, 13, bodyDark);
  framebuf.drawFastVLine(cx, y - 6, 10, white);

  // Difusor e luz de chuva traseira.
  framebuf.fillTriangle(cx - 9, y + 7, cx - 3, y + 13, cx - 1, y + 7, black);
  framebuf.fillTriangle(cx + 9, y + 7, cx + 3, y + 13, cx + 1, y + 7, black);
  framebuf.fillRect(cx - 2, y + 8, 4, 3, black);
  if (((frameNo / 6) & 1) == 0) framebuf.fillRect(cx - 1, y + 8, 2, 2, yellow);

  // Pequenos destaques para dar volume ao carro.
  framebuf.drawFastHLine(cx - 9, y, 7, white);
  framebuf.drawFastHLine(cx + 3, y, 7, white);
}

void drawPlayer() {
  drawF1Player(80, SCENE_H - 17);
}

void renderFrame() {
  drawRoadBase();
  drawLaneMarkers();
  drawRoadsidePosts();
  drawEnemy();
  drawPlayer();

  // Uma unica transferencia SPI grande por quadro.
  tft.drawRGBBitmap(0, SCENE_Y, framebuf.getBuffer(), 160, SCENE_H);
}

void drawHeader() {
  tft.fillRect(0, 0, 160, SCENE_Y, ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 4);
  tft.print("ENDURO PERFORMANCE TEST");
  tft.setCursor(4, 15);
  tft.print("FPS: ");
  tft.fillRect(30, 15, 28, 8, ST77XX_BLACK);
  tft.setCursor(30, 15);
  tft.print(fpsValue);
}

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  fpsStart = millis();
  lastFrame = millis();
  drawHeader();
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    frameNo++;
    renderFrame();
    fpsFrames++;
  }

  if (now - fpsStart >= 1000UL) {
    fpsValue = fpsFrames;
    fpsFrames = 0;
    fpsStart = now;

    tft.fillRect(30, 15, 28, 8, ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(1);
    tft.setCursor(30, 15);
    tft.print(fpsValue);

    Serial.print("FPS: ");
    Serial.println(fpsValue);
  }

  yield();
}

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

// Sprites simples pre-renderizados em 1 bit.
// 16x12 carro visto de tras.
const uint8_t PROGMEM CAR_PLAYER[] = {
  0x03,0xC0,
  0x07,0xE0,
  0x0F,0xF0,
  0x1F,0xF8,
  0x3F,0xFC,
  0x7F,0xFE,
  0xFF,0xFF,
  0xE7,0xE7,
  0xC3,0xC3,
  0xC3,0xC3,
  0x81,0x81,
  0x81,0x81
};

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
  // y 0..81: horizonte estreito em cima, largo embaixo.
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

  // Estrada em uma unica forma grande.
  framebuf.fillTriangle(78, 17, 18, SCENE_H - 1, 80, SCENE_H - 1, road);
  framebuf.fillTriangle(82, 17, 80, SCENE_H - 1, 142, SCENE_H - 1, road);
  framebuf.fillRect(78, 17, 5, 3, road);

  framebuf.drawLine(78, 17, 18, SCENE_H - 1, snow);
  framebuf.drawLine(82, 17, 142, SCENE_H - 1, snow);
}

void drawLaneMarkers() {
  const uint16_t yellow = ST77XX_YELLOW;
  const int phase = (frameNo * 4) % 32;

  // Segmentos aumentam conforme descem, criando perspectiva.
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
  // Movimento pseudo-3D sem float.
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

void drawPlayer() {
  // Jogador quase fixo; leve oscilacao de 1 px para dar vida.
  int bob = ((frameNo / 4) & 1) ? 1 : 0;
  drawBitmapScaled1bit(64, SCENE_H - 25 + bob, CAR_PLAYER, 16, 12,
                       ST77XX_WHITE, 2);
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
    // Evita acumular atraso se um frame demorar demais.
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

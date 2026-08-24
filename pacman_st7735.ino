#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ============================================================
// Pac-Man ST7735 - teste de fluidez
// NodeMCU ESP8266 + ST7735 1.8" 160x128
//
// Sequencia:
// 1) Fantasmas perseguem Pac-Man.
// 2) Pac-Man come a pastilha de energia.
// 3) Fantasmas ficam azuis e fogem.
// 4) Pac-Man volta perseguindo os fantasmas.
//
// Sem Wi-Fi / HTTPS / LittleFS para priorizar FPS.
// ============================================================

const uint16_t FRAME_MS = 33; // alvo ~30 FPS
const int SCENE_Y = 18;
const int SCENE_H = 92;

GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0;
uint16_t fpsValue = 0;

// Estados da animacao.
enum ChaseState : uint8_t {
  CHASE_GHOSTS = 0,
  POWER_PELLET = 1,
  CHASE_PACMAN = 2
};

ChaseState state = CHASE_GHOSTS;
unsigned long stateStart = 0;

// Posicoes principais.
int pacX = 118;
int pacDir = 1; // 1 direita, -1 esquerda
int ghostBaseX = 84;
int corridorY = 48;

// ============================================================
// Cores
// ============================================================

uint16_t BLUE_WALL;
uint16_t PINK_GHOST;
uint16_t CYAN_GHOST;
uint16_t ORANGE_GHOST;
uint16_t BLUE_FRIGHT;
uint16_t EYE_BLUE;

// ============================================================
// Labirinto leve
// ============================================================

void drawMaze() {
  framebuf.fillScreen(ST77XX_BLACK);

  // Bordas principais.
  framebuf.drawRect(1, 1, 158, SCENE_H - 2, BLUE_WALL);
  framebuf.drawRect(4, 4, 152, SCENE_H - 8, BLUE_WALL);

  // Estruturas fixas lembrando o labirinto original.
  framebuf.drawFastHLine(11, 18, 35, BLUE_WALL);
  framebuf.drawFastHLine(114, 18, 35, BLUE_WALL);
  framebuf.drawFastHLine(11, 72, 35, BLUE_WALL);
  framebuf.drawFastHLine(114, 72, 35, BLUE_WALL);

  framebuf.drawFastVLine(53, 8, 22, BLUE_WALL);
  framebuf.drawFastVLine(106, 8, 22, BLUE_WALL);
  framebuf.drawFastVLine(53, 62, 20, BLUE_WALL);
  framebuf.drawFastVLine(106, 62, 20, BLUE_WALL);

  framebuf.drawRect(67, 13, 26, 13, BLUE_WALL);
  framebuf.drawRect(67, 66, 26, 13, BLUE_WALL);

  // Tunel central livre.
  framebuf.drawFastHLine(4, 38, 36, BLUE_WALL);
  framebuf.drawFastHLine(120, 38, 36, BLUE_WALL);
  framebuf.drawFastHLine(4, 58, 36, BLUE_WALL);
  framebuf.drawFastHLine(120, 58, 36, BLUE_WALL);

  // Pellets normais no corredor central.
  int offset = (frameNo * 2) % 16;
  for (int x = -16; x < 176; x += 16) {
    int xx = x - offset;
    if (xx > 6 && xx < 154) framebuf.fillCircle(xx, corridorY, 1, ST77XX_WHITE);
  }
}

// ============================================================
// Sprites leves
// ============================================================

void drawPacman(int x, int y, int dir, bool mouthOpen) {
  framebuf.fillCircle(x, y, 10, ST77XX_YELLOW);

  if (mouthOpen) {
    if (dir > 0) {
      framebuf.fillTriangle(x + 1, y, x + 12, y - 7, x + 12, y + 7, ST77XX_BLACK);
    } else {
      framebuf.fillTriangle(x - 1, y, x - 12, y - 7, x - 12, y + 7, ST77XX_BLACK);
    }
  } else {
    if (dir > 0) framebuf.drawFastHLine(x + 2, y, 9, ST77XX_BLACK);
    else framebuf.drawFastHLine(x - 10, y, 9, ST77XX_BLACK);
  }

  int eyeX = x + (dir > 0 ? 2 : -2);
  framebuf.fillCircle(eyeX, y - 5, 1, ST77XX_BLACK);
}

void drawGhost(int x, int y, uint16_t color, int dir, bool frightened, uint8_t legPhase) {
  // Corpo.
  framebuf.fillCircle(x, y - 4, 9, color);
  framebuf.fillRect(x - 9, y - 4, 18, 12, color);

  // Base ondulada em dois frames.
  if (legPhase == 0) {
    framebuf.fillTriangle(x - 9, y + 8, x - 5, y + 4, x - 1, y + 8, color);
    framebuf.fillTriangle(x - 1, y + 8, x + 3, y + 4, x + 7, y + 8, color);
    framebuf.fillTriangle(x + 7, y + 8, x + 9, y + 4, x + 9, y + 8, color);
  } else {
    framebuf.fillTriangle(x - 9, y + 4, x - 6, y + 8, x - 3, y + 4, color);
    framebuf.fillTriangle(x - 3, y + 4, x, y + 8, x + 3, y + 4, color);
    framebuf.fillTriangle(x + 3, y + 4, x + 6, y + 8, x + 9, y + 4, color);
  }

  if (frightened) {
    // Olhos e boca simples de fantasma assustado.
    framebuf.fillCircle(x - 4, y - 4, 2, ST77XX_WHITE);
    framebuf.fillCircle(x + 4, y - 4, 2, ST77XX_WHITE);
    framebuf.drawPixel(x - 5, y + 3, ST77XX_WHITE);
    framebuf.drawPixel(x - 2, y + 1, ST77XX_WHITE);
    framebuf.drawPixel(x + 1, y + 3, ST77XX_WHITE);
    framebuf.drawPixel(x + 4, y + 1, ST77XX_WHITE);
  } else {
    framebuf.fillCircle(x - 4, y - 4, 3, ST77XX_WHITE);
    framebuf.fillCircle(x + 4, y - 4, 3, ST77XX_WHITE);
    int pupilDx = dir > 0 ? 1 : -1;
    framebuf.fillCircle(x - 4 + pupilDx, y - 4, 1, EYE_BLUE);
    framebuf.fillCircle(x + 4 + pupilDx, y - 4, 1, EYE_BLUE);
  }
}

// ============================================================
// Sequencia de perseguicao
// ============================================================

void updateState() {
  unsigned long now = millis();
  unsigned long elapsed = now - stateStart;

  switch (state) {
    case CHASE_GHOSTS:
      // 7 segundos de fantasmas perseguindo Pac-Man.
      if (elapsed >= 7000UL) {
        state = POWER_PELLET;
        stateStart = now;
      }
      break;

    case POWER_PELLET:
      // Curta pausa para destacar a pastilha de energia.
      if (elapsed >= 900UL) {
        state = CHASE_PACMAN;
        stateStart = now;
      }
      break;

    case CHASE_PACMAN:
      // 7 segundos de Pac-Man perseguindo os fantasmas azuis.
      if (elapsed >= 7000UL) {
        state = CHASE_GHOSTS;
        stateStart = now;
      }
      break;
  }
}

void drawChaseScene() {
  drawMaze();

  bool mouthOpen = ((frameNo / 2) & 1) == 0;
  uint8_t legPhase = (frameNo / 3) & 1;

  if (state == CHASE_GHOSTS) {
    // Tudo se move para a direita. Pac-Man esta na frente.
    int swing = (frameNo / 2) % 10;

    pacDir = 1;
    pacX = 118 + (swing < 5 ? swing : 9 - swing);

    int g1 = pacX - 31;
    int g2 = pacX - 55;
    int g3 = pacX - 79;

    drawPacman(pacX, corridorY, pacDir, mouthOpen);
    drawGhost(g1, corridorY, ST77XX_RED, 1, false, legPhase);
    drawGhost(g2, corridorY, PINK_GHOST, 1, false, legPhase);
    drawGhost(g3, corridorY, CYAN_GHOST, 1, false, legPhase);

    // Pastilha de energia fica mais a frente, aguardando o momento da virada.
    framebuf.fillCircle(149, corridorY, 4, ST77XX_WHITE);

  } else if (state == POWER_PELLET) {
    // Pac-Man chega na pastilha e a consome.
    pacDir = 1;
    pacX = 139;
    drawPacman(pacX, corridorY, pacDir, mouthOpen);

    // Pisca a energia pouco antes da inversao.
    if (((frameNo / 3) & 1) == 0) framebuf.fillCircle(151, corridorY, 5, ST77XX_WHITE);

    drawGhost(101, corridorY, ST77XX_RED, 1, false, legPhase);
    drawGhost(77, corridorY, PINK_GHOST, 1, false, legPhase);
    drawGhost(53, corridorY, CYAN_GHOST, 1, false, legPhase);

  } else {
    // Pac-Man volta para a esquerda, agora perseguindo os fantasmas azuis.
    int swing = (frameNo / 2) % 10;

    pacDir = -1;
    pacX = 44 - (swing < 5 ? swing : 9 - swing);

    int g1 = pacX - 34;
    int g2 = pacX - 59;
    int g3 = pacX - 84;

    // Quando saem pela esquerda, reaparecem pela direita, simulando o tunel.
    if (g1 < -10) g1 += 180;
    if (g2 < -10) g2 += 180;
    if (g3 < -10) g3 += 180;

    drawPacman(pacX, corridorY, pacDir, mouthOpen);
    drawGhost(g1, corridorY, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(g2, corridorY, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(g3, corridorY, BLUE_FRIGHT, -1, true, legPhase);
  }

  // Uma unica transferencia SPI grande por frame.
  tft.drawRGBBitmap(0, SCENE_Y, framebuf.getBuffer(), 160, SCENE_H);
}

void drawHeader() {
  tft.fillRect(0, 0, 160, SCENE_Y, ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(4, 3);
  tft.print("PAC-MAN ST7735");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(103, 3);
  tft.print("FPS:");

  tft.fillRect(132, 3, 26, 8, ST77XX_BLACK);
  tft.setCursor(132, 3);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(fpsValue);
}

void setup() {
  Serial.begin(115200);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  BLUE_WALL = tft.color565(25, 50, 235);
  PINK_GHOST = tft.color565(255, 125, 190);
  CYAN_GHOST = tft.color565(70, 220, 255);
  ORANGE_GHOST = tft.color565(255, 165, 45);
  BLUE_FRIGHT = tft.color565(35, 55, 220);
  EYE_BLUE = tft.color565(30, 70, 255);

  lastFrame = millis();
  fpsStart = millis();
  stateStart = millis();

  drawHeader();
}

void loop() {
  unsigned long now = millis();

  updateState();

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    frameNo++;
    drawChaseScene();
    fpsFrames++;
  }

  if (now - fpsStart >= 1000UL) {
    fpsValue = fpsFrames;
    fpsFrames = 0;
    fpsStart = now;

    tft.fillRect(132, 3, 26, 8, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(132, 3);
    tft.print(fpsValue);

    Serial.print("FPS: ");
    Serial.println(fpsValue);
  }

  yield();
}

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ============================================================
// Pac-Man ST7735 - travessia completa e fluida
// NodeMCU ESP8266 + ST7735 1.8" 160x128
//
// Sequencia:
// 1) Fantasmas perseguem Pac-Man da esquerda para a direita.
// 2) Pac-Man atravessa a tela inteira e come a pastilha de energia.
// 3) Fantasmas ficam azuis.
// 4) Todos voltam da direita para a esquerda, com Pac-Man perseguindo.
// 5) Ao sair completamente da tela, o ciclo reinicia.
//
// Sem Wi-Fi/HTTPS/LittleFS para priorizar FPS.
// ============================================================

const uint16_t FRAME_MS = 33; // alvo ~30 FPS
const int SCENE_Y = 18;
const int SCENE_H = 92;
const int CORRIDOR_Y = 47;

GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0;
uint16_t fpsValue = 0;

uint16_t BLUE_WALL;
uint16_t PINK_GHOST;
uint16_t CYAN_GHOST;
uint16_t ORANGE_GHOST;
uint16_t BLUE_FRIGHT;
uint16_t EYE_BLUE;

// ============================================================
// Estados
// ============================================================

enum ChaseState : uint8_t {
  RUN_RIGHT = 0,     // fantasmas perseguem Pac-Man
  POWER_FLASH = 1,  // energia / inversao
  RUN_LEFT = 2      // Pac-Man persegue fantasmas azuis
};

ChaseState state = RUN_RIGHT;
unsigned long stateStart = 0;

// Posicao-base do grupo.
// RUN_RIGHT: cresce ate todo mundo sair pela direita.
// RUN_LEFT: decresce ate todo mundo sair pela esquerda.
int groupX = -85;

// ============================================================
// Labirinto leve
// ============================================================

void drawMaze() {
  framebuf.fillScreen(ST77XX_BLACK);

  framebuf.drawRect(1, 1, 158, SCENE_H - 2, BLUE_WALL);
  framebuf.drawRect(4, 4, 152, SCENE_H - 8, BLUE_WALL);

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

  // Tunel central visualmente livre.
  framebuf.drawFastHLine(4, 37, 36, BLUE_WALL);
  framebuf.drawFastHLine(120, 37, 36, BLUE_WALL);
  framebuf.drawFastHLine(4, 58, 36, BLUE_WALL);
  framebuf.drawFastHLine(120, 58, 36, BLUE_WALL);

  // Pellets fixos. Nao rolamos o fundo: os personagens realmente atravessam a tela.
  for (int x = 10; x <= 150; x += 14) {
    framebuf.fillCircle(x, CORRIDOR_Y, 1, ST77XX_WHITE);
  }

  // Power pellet no extremo direito durante a ida.
  if (state == RUN_RIGHT || state == POWER_FLASH) {
    if (state != POWER_FLASH || ((frameNo / 3) & 1) == 0) {
      framebuf.fillCircle(149, CORRIDOR_Y, 4, ST77XX_WHITE);
    }
  }
}

// ============================================================
// Sprites
// ============================================================

void drawPacman(int x, int y, int dir, bool mouthOpen) {
  framebuf.fillCircle(x, y, 10, ST77XX_YELLOW);

  if (mouthOpen) {
    if (dir > 0)
      framebuf.fillTriangle(x + 1, y, x + 12, y - 7, x + 12, y + 7, ST77XX_BLACK);
    else
      framebuf.fillTriangle(x - 1, y, x - 12, y - 7, x - 12, y + 7, ST77XX_BLACK);
  } else {
    if (dir > 0) framebuf.drawFastHLine(x + 2, y, 9, ST77XX_BLACK);
    else framebuf.drawFastHLine(x - 10, y, 9, ST77XX_BLACK);
  }

  framebuf.fillCircle(x + (dir > 0 ? 2 : -2), y - 5, 1, ST77XX_BLACK);
}

void drawGhost(int x, int y, uint16_t color, int dir, bool frightened, uint8_t legPhase) {
  framebuf.fillCircle(x, y - 4, 9, color);
  framebuf.fillRect(x - 9, y - 4, 18, 12, color);

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
    framebuf.fillCircle(x - 4, y - 4, 2, ST77XX_WHITE);
    framebuf.fillCircle(x + 4, y - 4, 2, ST77XX_WHITE);
    framebuf.drawPixel(x - 5, y + 3, ST77XX_WHITE);
    framebuf.drawPixel(x - 2, y + 1, ST77XX_WHITE);
    framebuf.drawPixel(x + 1, y + 3, ST77XX_WHITE);
    framebuf.drawPixel(x + 4, y + 1, ST77XX_WHITE);
  } else {
    framebuf.fillCircle(x - 4, y - 4, 3, ST77XX_WHITE);
    framebuf.fillCircle(x + 4, y - 4, 3, ST77XX_WHITE);
    int dx = dir > 0 ? 1 : -1;
    framebuf.fillCircle(x - 4 + dx, y - 4, 1, EYE_BLUE);
    framebuf.fillCircle(x + 4 + dx, y - 4, 1, EYE_BLUE);
  }
}

// ============================================================
// Movimento
// ============================================================

void updateAnimation() {
  unsigned long now = millis();

  if (state == RUN_RIGHT) {
    // Pac-Man fica 80 px a frente do ultimo fantasma.
    // groupX e a posicao do fantasma de tras.
    groupX += 2;

    // Pac-Man chega ao power pellet por volta de x=149.
    // pac = groupX + 82 => groupX ~= 67.
    if (groupX >= 67) {
      state = POWER_FLASH;
      stateStart = now;
    }

  } else if (state == POWER_FLASH) {
    // Pequena pausa dramatica para a energia.
    if (now - stateStart >= 450UL) {
      state = RUN_LEFT;

      // Comeca com os fantasmas na frente, ja no lado direito.
      // Assim todos atravessam a tela inteira no retorno.
      groupX = 205;
    }

  } else { // RUN_LEFT
    groupX -= 2;

    // O Pac-Man e o ultimo elemento do grupo no retorno.
    // So reinicia quando ele tambem saiu totalmente pela esquerda.
    if (groupX < -95) {
      state = RUN_RIGHT;
      groupX = -85;
      stateStart = now;
    }
  }
}

void drawScene() {
  drawMaze();

  bool mouthOpen = ((frameNo / 2) & 1) == 0;
  uint8_t legPhase = (frameNo / 3) & 1;

  if (state == RUN_RIGHT) {
    // Da esquerda para a direita:
    // vermelho -> rosa -> ciano -> Pac-Man
    int g3 = groupX;
    int g2 = groupX + 24;
    int g1 = groupX + 48;
    int pac = groupX + 82;

    drawGhost(g3, CORRIDOR_Y, ST77XX_RED, 1, false, legPhase);
    drawGhost(g2, CORRIDOR_Y, PINK_GHOST, 1, false, legPhase);
    drawGhost(g1, CORRIDOR_Y, CYAN_GHOST, 1, false, legPhase);
    drawPacman(pac, CORRIDOR_Y, 1, mouthOpen);

  } else if (state == POWER_FLASH) {
    // Pac-Man sobre a pastilha; fantasmas ainda atras.
    drawGhost(76, CORRIDOR_Y, ST77XX_RED, 1, false, legPhase);
    drawGhost(100, CORRIDOR_Y, PINK_GHOST, 1, false, legPhase);
    drawGhost(124, CORRIDOR_Y, CYAN_GHOST, 1, false, legPhase);
    drawPacman(145, CORRIDOR_Y, 1, mouthOpen);

  } else {
    // Da direita para a esquerda:
    // fantasmas azuis na frente; Pac-Man atras perseguindo.
    int g3 = groupX;
    int g2 = groupX + 24;
    int g1 = groupX + 48;
    int pac = groupX + 82;

    drawGhost(g3, CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(g2, CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(g1, CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawPacman(pac, CORRIDOR_Y, -1, mouthOpen);
  }

  // Uma unica transferencia SPI por quadro.
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

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    frameNo++;
    updateAnimation();
    drawScene();
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

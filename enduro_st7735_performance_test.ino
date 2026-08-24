#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Enduro Atari-style performance test.
// Sem Wi-Fi/HTTPS: objetivo e testar fluidez e logica de desvio automatico.

const uint16_t FRAME_MS = 33; // alvo ~30 FPS
const int SCENE_Y = 26;
const int SCENE_H = 82;
const int PLAYER_Y = SCENE_H - 13;

GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0;
uint16_t fpsValue = 0;

struct Enemy {
  int8_t lane;      // 0 esquerda, 1 centro, 2 direita
  int16_t y;
  uint8_t speed;
  uint16_t color;
};

const uint8_t ENEMY_COUNT = 5;
Enemy enemies[ENEMY_COUNT];

int8_t playerLane = 2;
int8_t targetLane = 2;
int16_t playerX = 116;

inline int roadLeftAt(int y) {
  return 78 - (y * 60) / (SCENE_H - 1);
}

inline int roadRightAt(int y) {
  return 82 + (y * 60) / (SCENE_H - 1);
}

int laneCenter(int lane, int y) {
  int left = roadLeftAt(y);
  int right = roadRightAt(y);
  int width = right - left;

  if (lane == 0) return left + width / 4;
  if (lane == 1) return (left + right) / 2;
  return right - width / 4;
}

uint16_t enemyColor(uint8_t n) {
  switch (n % 4) {
    case 0: return tft.color565(226, 205, 55);   // amarelo Atari
    case 1: return tft.color565(70, 210, 220);   // ciano
    case 2: return tft.color565(235, 105, 55);   // laranja
    default:return tft.color565(220, 120, 205);  // rosa
  }
}

void spawnEnemy(uint8_t i, int yStart) {
  enemies[i].lane = random(0, 3);
  enemies[i].y = yStart;
  enemies[i].speed = 1 + random(0, 2);
  enemies[i].color = enemyColor(i + random(0, 4));
}

void initEnemies() {
  spawnEnemy(0, -5);
  spawnEnemy(1, 10);
  spawnEnemy(2, 24);
  spawnEnemy(3, -22);
  spawnEnemy(4, 37);
}

void drawRoad() {
  const uint16_t green = tft.color565(0, 86, 18);
  const uint16_t greenRoad = tft.color565(0, 78, 15);
  const uint16_t white = tft.color565(220, 220, 220);

  framebuf.fillScreen(green);

  // Pista verde, como no Enduro original.
  for (int y = 0; y < SCENE_H; y++) {
    int left = roadLeftAt(y);
    int right = roadRightAt(y);
    framebuf.drawFastHLine(left, y, right - left + 1, greenRoad);
  }

  // Bordas claras em perspectiva.
  for (int y = 1; y < SCENE_H; y++) {
    framebuf.drawLine(roadLeftAt(y - 1), y - 1, roadLeftAt(y), y, white);
    framebuf.drawLine(roadRightAt(y - 1), y - 1, roadRightAt(y), y, white);
  }

  // Pequenas irregularidades/curvas visuais nas bordas.
  int phase = (frameNo / 6) % 24;
  if (phase < 12) {
    framebuf.drawFastHLine(roadLeftAt(40) - 2, 40, 3, white);
    framebuf.drawFastHLine(roadRightAt(28) - 1, 28, 3, white);
  } else {
    framebuf.drawFastHLine(roadLeftAt(28) - 2, 28, 3, white);
    framebuf.drawFastHLine(roadRightAt(42) - 1, 42, 3, white);
  }
}

void drawAtariCar(int cx, int cy, uint16_t color, uint8_t scale, bool player) {
  // Forma achatada e horizontal inspirada no sprite de Enduro.
  int bodyW = 12 * scale;
  int bodyH = 5 * scale;
  int noseW = 6 * scale;

  framebuf.fillRect(cx - bodyW / 2, cy - bodyH / 2, bodyW, bodyH, color);
  framebuf.fillRect(cx - noseW / 2, cy - bodyH / 2 - 2 * scale,
                    noseW, 2 * scale, color);

  // Extensoes laterais / rodas pixeladas.
  framebuf.fillRect(cx - bodyW / 2 - 3 * scale, cy - scale,
                    3 * scale, scale, color);
  framebuf.fillRect(cx - bodyW / 2 - 4 * scale, cy + scale,
                    4 * scale, scale, color);
  framebuf.fillRect(cx + bodyW / 2, cy - scale,
                    3 * scale, scale, color);
  framebuf.fillRect(cx + bodyW / 2, cy + scale,
                    4 * scale, scale, color);

  if (player) {
    // Recorte central discreto para o carro branco parecer mais com o original.
    framebuf.fillRect(cx - scale, cy - bodyH / 2 - scale,
                      2 * scale, 2 * scale, ST77XX_BLACK);
  }
}

bool dangerInLane(int lane) {
  for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
    if (enemies[i].lane != lane) continue;
    if (enemies[i].y >= 43 && enemies[i].y <= PLAYER_Y + 7) return true;
  }
  return false;
}

int dangerScore(int lane) {
  int score = 0;
  for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
    if (enemies[i].lane != lane) continue;
    int d = PLAYER_Y - enemies[i].y;
    if (d >= -6 && d < 38) score += (40 - max(0, d));
  }
  return score;
}

void chooseLane() {
  if (dangerInLane(playerLane)) {
    int bestLane = playerLane;
    int bestScore = 9999;

    for (int lane = 0; lane < 3; lane++) {
      if (lane == playerLane) continue;
      int score = dangerScore(lane);
      if (score < bestScore) {
        bestScore = score;
        bestLane = lane;
      }
    }
    targetLane = bestLane;
  } else {
    // Mantem movimento menos robotico: prefere direita se estiver livre.
    if (playerLane != 2 && !dangerInLane(2) && (frameNo % 150) == 0) {
      targetLane = 2;
    }
  }
}

void movePlayer() {
  int desired = laneCenter(targetLane, PLAYER_Y);
  const int step = 2;

  if (playerX < desired) playerX += step;
  else if (playerX > desired) playerX -= step;

  if (abs(playerX - desired) <= step) {
    playerX = desired;
    playerLane = targetLane;
  }
}

void updateEnemies() {
  for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
    enemies[i].y += enemies[i].speed;

    if (enemies[i].y > SCENE_H + 12) {
      spawnEnemy(i, random(-28, 3));
    }
  }
}

void drawEnemies() {
  // Longe = pequeno; perto = maior, criando profundidade.
  for (uint8_t i = 0; i < ENEMY_COUNT; i++) {
    int y = enemies[i].y;
    if (y < 2 || y >= SCENE_H - 2) continue;

    uint8_t scale = 1;
    if (y > 52) scale = 2;

    int x = laneCenter(enemies[i].lane, y);
    drawAtariCar(x, y, enemies[i].color, scale, false);
  }
}

void drawPlayer() {
  uint16_t playerColor = tft.color565(224, 214, 224);
  int bob = ((frameNo / 5) & 1) ? 1 : 0;
  drawAtariCar(playerX, PLAYER_Y + bob, playerColor, 2, true);
}

void renderFrame() {
  drawRoad();
  drawEnemies();
  drawPlayer();

  // Uma unica transferencia SPI por quadro.
  tft.drawRGBBitmap(0, SCENE_Y, framebuf.getBuffer(), 160, SCENE_H);
}

void drawHeader() {
  tft.fillRect(0, 0, 160, SCENE_Y, ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 4);
  tft.print("ENDURO ATARI TEST");
  tft.setCursor(4, 15);
  tft.print("FPS: ");
  tft.fillRect(30, 15, 28, 8, ST77XX_BLACK);
  tft.setCursor(30, 15);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(fpsValue);
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  playerX = laneCenter(playerLane, PLAYER_Y);
  initEnemies();

  fpsStart = millis();
  lastFrame = millis();
  drawHeader();
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    frameNo++;

    updateEnemies();
    chooseLane();
    movePlayer();
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

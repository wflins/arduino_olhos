#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ============================================================
// Pac-Man ST7735 - travessia real de ponta a ponta
// NodeMCU ESP8266 + ST7735 1.8" 160x128
//
// 1) Grupo entra totalmente pela esquerda e corre para a direita.
// 2) Pac-Man come a pastilha de energia no lado direito.
// 3) Fantasmas ficam azuis e o MESMO grupo inverte o sentido.
// 4) Todos atravessam a tela para a esquerda.
// 5) O ciclo so reinicia quando todos sairam completamente.
//
// Sem Wi-Fi/HTTPS/LittleFS para priorizar FPS.
// ============================================================

const uint16_t FRAME_MS = 33; // ~30 FPS
const int SCENE_Y = 18;
const int SCENE_H = 92;
const int CORRIDOR_Y = 47;
const int SPEED_PX = 3;       // movimento bem visivel e ainda suave

GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0;
uint16_t fpsValue = 0;

uint16_t BLUE_WALL;
uint16_t PINK_GHOST;
uint16_t CYAN_GHOST;
uint16_t BLUE_FRIGHT;
uint16_t EYE_BLUE;

enum ChaseState : uint8_t {
  RUN_RIGHT = 0,
  POWER_FLASH = 1,
  RUN_LEFT = 2
};

ChaseState state = RUN_RIGHT;
unsigned long stateStart = 0;

// groupX e SEMPRE a posicao do fantasma vermelho/azul mais atras.
// Todas as demais posicoes sao calculadas a partir dele.
// Nao existem personagens fixos na tela.
int groupX = -115;

// Offsets do grupo na ida e na volta.
const int OFF_GHOST_RED  = 0;
const int OFF_GHOST_PINK = 26;
const int OFF_GHOST_CYAN = 52;
const int OFF_PAC        = 88;

void drawMaze() {
  framebuf.fillScreen(ST77XX_BLACK);

  framebuf.drawRect(1, 1, 158, SCENE_H - 2, BLUE_WALL);
  framebuf.drawRect(4, 4, 152, SCENE_H - 8, BLUE_WALL);

  // Paredes leves, deixando um tunel central totalmente aberto.
  framebuf.drawFastHLine(12, 17, 38, BLUE_WALL);
  framebuf.drawFastHLine(110, 17, 38, BLUE_WALL);
  framebuf.drawFastHLine(12, 73, 38, BLUE_WALL);
  framebuf.drawFastHLine(110, 73, 38, BLUE_WALL);
  framebuf.drawFastVLine(55, 7, 22, BLUE_WALL);
  framebuf.drawFastVLine(104, 7, 22, BLUE_WALL);
  framebuf.drawFastVLine(55, 63, 20, BLUE_WALL);
  framebuf.drawFastVLine(104, 63, 20, BLUE_WALL);
  framebuf.drawRect(69, 12, 22, 14, BLUE_WALL);
  framebuf.drawRect(69, 66, 22, 14, BLUE_WALL);

  // Pellets fixos para deixar evidente que os personagens estao se deslocando.
  for (int x = 10; x <= 146; x += 14) {
    framebuf.fillCircle(x, CORRIDOR_Y, 1, ST77XX_WHITE);
  }

  // Power pellet no extremo direito durante a ida.
  if (state == RUN_RIGHT || state == POWER_FLASH) {
    bool mostra = state == RUN_RIGHT || (((frameNo / 2) & 1) == 0);
    if (mostra) framebuf.fillCircle(149, CORRIDOR_Y, 4, ST77XX_WHITE);
  }
}

void drawPacman(int x, int y, int dir, bool mouthOpen) {
  // clipping natural do GFXcanvas: pode entrar/sair pelas bordas suavemente
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

void updateAnimation() {
  unsigned long now = millis();

  if (state == RUN_RIGHT) {
    groupX += SPEED_PX;

    // Pac-Man = groupX + 88. Quando chega ao pellet x~149,
    // groupX fica perto de 61. O grupo inteiro esteve se movendo o tempo todo.
    if (groupX + OFF_PAC >= 148) {
      state = POWER_FLASH;
      stateStart = now;
    }
  }
  else if (state == POWER_FLASH) {
    // Nao congela os personagens: ainda avancam lentamente enquanto a energia pisca.
    if ((frameNo & 1) == 0) groupX += 1;

    if (now - stateStart >= 260UL) {
      state = RUN_LEFT;
      stateStart = now;
    }
  }
  else { // RUN_LEFT
    groupX -= SPEED_PX;

    // Pac-Man e o personagem mais a direita. So reinicia quando ele tambem
    // saiu completamente pela esquerda, garantindo travessia de ponta a ponta.
    if (groupX + OFF_PAC < -14) {
      state = RUN_RIGHT;
      groupX = -115;
      stateStart = now;
    }
  }
}

void drawScene() {
  drawMaze();

  bool mouthOpen = ((frameNo / 2) & 1) == 0;
  uint8_t legPhase = (frameNo / 3) & 1;

  int redX  = groupX + OFF_GHOST_RED;
  int pinkX = groupX + OFF_GHOST_PINK;
  int cyanX = groupX + OFF_GHOST_CYAN;
  int pacX  = groupX + OFF_PAC;

  if (state == RUN_RIGHT || state == POWER_FLASH) {
    // Fantasmas atras; Pac-Man na frente. Todos movem para a direita.
    drawGhost(redX,  CORRIDOR_Y, ST77XX_RED,  1, false, legPhase);
    drawGhost(pinkX, CORRIDOR_Y, PINK_GHOST,  1, false, legPhase);
    drawGhost(cyanX, CORRIDOR_Y, CYAN_GHOST,  1, false, legPhase);
    drawPacman(pacX, CORRIDOR_Y, 1, mouthOpen);
  } else {
    // Ao inverter o sentido, os fantasmas que estavam atras passam a estar
    // na frente na direcao da esquerda. Pac-Man vem atras perseguindo-os.
    drawGhost(redX,  CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(pinkX, CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawGhost(cyanX, CORRIDOR_Y, BLUE_FRIGHT, -1, true, legPhase);
    drawPacman(pacX, CORRIDOR_Y, -1, mouthOpen);
  }

  // Uma unica transferencia SPI por quadro para manter a fluidez.
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

  BLUE_WALL   = tft.color565(25, 50, 235);
  PINK_GHOST  = tft.color565(255, 125, 190);
  CYAN_GHOST  = tft.color565(70, 220, 255);
  BLUE_FRIGHT = tft.color565(35, 55, 220);
  EYE_BLUE    = tft.color565(30, 70, 255);

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

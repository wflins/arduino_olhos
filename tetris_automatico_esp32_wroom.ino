#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_random.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000
#define FRAME_INTERVAL 120

#define COLS 10
#define ROWS 18
#define CELL 3
#define BOARD_X 49
#define BOARD_Y 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool board[ROWS][COLS];
unsigned long ultimoFrame = 0;
int pecaX = 4;
int pecaY = -2;
int tipo = 0;
int rot = 0;

const int8_t shapes[5][4][2] = {
  {{0,0},{1,0},{0,1},{1,1}},       // O
  {{-1,0},{0,0},{1,0},{2,0}},      // I
  {{-1,0},{0,0},{1,0},{0,1}},      // T
  {{-1,0},{0,0},{0,1},{1,1}},      // S
  {{-1,1},{-1,0},{0,0},{1,0}}      // L
};

void blocoRotacionado(int i, int &x, int &y) {
  x = shapes[tipo][i][0];
  y = shapes[tipo][i][1];
  for (int r = 0; r < rot; r++) {
    int t = x;
    x = -y;
    y = t;
  }
}

bool colisao(int nx, int ny, int nrot) {
  int oldRot = rot;
  rot = nrot;
  for (int i = 0; i < 4; i++) {
    int dx, dy;
    blocoRotacionado(i, dx, dy);
    int x = nx + dx;
    int y = ny + dy;
    if (x < 0 || x >= COLS || y >= ROWS) { rot = oldRot; return true; }
    if (y >= 0 && board[y][x]) { rot = oldRot; return true; }
  }
  rot = oldRot;
  return false;
}

void fixarPeca() {
  for (int i = 0; i < 4; i++) {
    int dx, dy;
    blocoRotacionado(i, dx, dy);
    int x = pecaX + dx;
    int y = pecaY + dy;
    if (x >= 0 && x < COLS && y >= 0 && y < ROWS) board[y][x] = true;
  }
}

void limparLinhas() {
  for (int y = ROWS - 1; y >= 0; y--) {
    bool cheia = true;
    for (int x = 0; x < COLS; x++) if (!board[y][x]) cheia = false;
    if (cheia) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < COLS; x++) board[yy][x] = board[yy - 1][x];
      for (int x = 0; x < COLS; x++) board[0][x] = false;
      y++;
    }
  }
}

void novaPeca() {
  tipo = random(0, 5);
  rot = random(0, 4);
  pecaX = random(2, 8);
  pecaY = -2;
  if (colisao(pecaX, pecaY, rot)) {
    memset(board, 0, sizeof(board));
  }
}

void desenharBloco(int x, int y, bool preenchido) {
  int px = BOARD_X + x * CELL;
  int py = BOARD_Y + y * CELL;
  if (preenchido) display.fillRect(px, py, CELL - 1, CELL - 1, SSD1306_WHITE);
}

void desenhar() {
  display.clearDisplay();
  display.drawRect(BOARD_X - 2, BOARD_Y - 2, COLS * CELL + 3, ROWS * CELL + 3, SSD1306_WHITE);

  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < COLS; x++)
      if (board[y][x]) desenharBloco(x, y, true);

  for (int i = 0; i < 4; i++) {
    int dx, dy;
    blocoRotacionado(i, dx, dy);
    int x = pecaX + dx;
    int y = pecaY + dy;
    if (y >= 0) desenharBloco(x, y, true);
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 8);
  display.print(F("TETRIS"));
  display.setCursor(2, 22);
  display.print(F("AUTO"));
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) delay(1000);
  }
  randomSeed(esp_random());
  memset(board, 0, sizeof(board));
  novaPeca();
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  // IA simples: tenta alinhar a peca em uma coluna aleatoria enquanto cai.
  if (random(0, 5) == 0) {
    int dir = random(0, 2) ? 1 : -1;
    if (!colisao(pecaX + dir, pecaY, rot)) pecaX += dir;
  }
  if (random(0, 14) == 0) {
    int nr = (rot + 1) % 4;
    if (!colisao(pecaX, pecaY, nr)) rot = nr;
  }

  if (!colisao(pecaX, pecaY + 1, rot)) {
    pecaY++;
  } else {
    fixarPeca();
    limparLinhas();
    novaPeca();
  }

  desenhar();
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define INV_ROWS 2
#define INV_COLS 5
#define PLAYER_Y 57
#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool invasorVivo[INV_ROWS][INV_COLS];
int grupoX = 10;
int grupoY = 7;
int direcaoGrupo = 1;

int jogadorX = 64;
int direcaoJogador = 1;

int tiroX = 0;
int tiroY = 0;
bool tiroAtivo = false;

int tiroInimigoX = 0;
int tiroInimigoY = 0;
bool tiroInimigoAtivo = false;

bool frameInvasor = false;
bool explosaoJogador = false;

unsigned long ultimoFrame = 0;
unsigned long ultimoPassoInvader = 0;
unsigned long ultimoTiro = 0;
unsigned long ultimoTiroInimigo = 0;
unsigned long inicioExplosao = 0;

void desenharInvader(int x, int y, bool fase) {
  display.drawLine(x + 2, y, x + 5, y, SSD1306_WHITE);
  display.drawPixel(x + 1, y + 1, SSD1306_WHITE);
  display.drawPixel(x + 6, y + 1, SSD1306_WHITE);
  display.drawLine(x, y + 2, x + 7, y + 2, SSD1306_WHITE);
  display.drawLine(x, y + 3, x + 7, y + 3, SSD1306_WHITE);

  display.drawPixel(x + 2, y + 3, SSD1306_BLACK);
  display.drawPixel(x + 5, y + 3, SSD1306_BLACK);

  display.drawPixel(x + 1, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 6, y + 4, SSD1306_WHITE);

  if (fase) {
    display.drawPixel(x, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 3, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 4, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 7, y + 5, SSD1306_WHITE);
  } else {
    display.drawPixel(x + 1, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 2, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 5, y + 5, SSD1306_WHITE);
    display.drawPixel(x + 6, y + 5, SSD1306_WHITE);
  }
}

void desenharCanhao(int x, int y) {
  display.fillRect(x - 6, y, 13, 3, SSD1306_WHITE);
  display.fillRect(x - 3, y - 3, 7, 3, SSD1306_WHITE);
  display.drawPixel(x, y - 4, SSD1306_WHITE);
}

void desenharExplosao(int x, int y) {
  display.drawLine(x - 7, y, x + 7, y, SSD1306_WHITE);
  display.drawLine(x, y - 7, x, y + 5, SSD1306_WHITE);
  display.drawLine(x - 5, y - 5, x + 5, y + 5, SSD1306_WHITE);
  display.drawLine(x + 5, y - 5, x - 5, y + 5, SSD1306_WHITE);
}

void reiniciarInvasores() {
  for (int r = 0; r < INV_ROWS; r++) {
    for (int c = 0; c < INV_COLS; c++) {
      invasorVivo[r][c] = true;
    }
  }

  grupoX = 10;
  grupoY = 7;
  direcaoGrupo = 1;
  tiroAtivo = false;
  tiroInimigoAtivo = false;
}

bool aindaHaInvasores() {
  for (int r = 0; r < INV_ROWS; r++) {
    for (int c = 0; c < INV_COLS; c++) {
      if (invasorVivo[r][c]) return true;
    }
  }
  return false;
}

void iniciarExplosao() {
  if (!explosaoJogador) {
    explosaoJogador = true;
    inicioExplosao = millis();
    tiroAtivo = false;
    tiroInimigoAtivo = false;
  }
}

void dispararJogador() {
  if (!tiroAtivo && !explosaoJogador) {
    tiroX = jogadorX;
    tiroY = PLAYER_Y - 5;
    tiroAtivo = true;
  }
}

void dispararInimigo() {
  if (tiroInimigoAtivo || explosaoJogador) return;

  for (int tentativa = 0; tentativa < 20; tentativa++) {
    int c = random(INV_COLS);

    for (int r = INV_ROWS - 1; r >= 0; r--) {
      if (invasorVivo[r][c]) {
        tiroInimigoX = grupoX + c * 20 + 4;
        tiroInimigoY = grupoY + r * 12 + 7;
        tiroInimigoAtivo = true;
        return;
      }
    }
  }
}

void verificarTiroJogador() {
  if (!tiroAtivo) return;

  for (int r = 0; r < INV_ROWS; r++) {
    for (int c = 0; c < INV_COLS; c++) {
      if (!invasorVivo[r][c]) continue;

      int x = grupoX + c * 20;
      int y = grupoY + r * 12;

      if (tiroX >= x && tiroX <= x + 7 && tiroY >= y && tiroY <= y + 6) {
        invasorVivo[r][c] = false;
        tiroAtivo = false;
        return;
      }
    }
  }
}

void atualizarJogo() {
  unsigned long agora = millis();

  if (explosaoJogador) {
    if (agora - inicioExplosao > 700) {
      explosaoJogador = false;
      jogadorX = 64;
      reiniciarInvasores();
    }
    return;
  }

  if (agora - ultimoPassoInvader >= 320) {
    ultimoPassoInvader = agora;
    frameInvasor = !frameInvasor;

    grupoX += direcaoGrupo * 3;

    if (grupoX <= 2 || grupoX + (INV_COLS - 1) * 20 + 8 >= SCREEN_WIDTH - 2) {
      direcaoGrupo *= -1;
      grupoY += 4;
    }
  }

  jogadorX += direcaoJogador * 2;
  if (jogadorX >= SCREEN_WIDTH - 8 || jogadorX <= 8) {
    direcaoJogador *= -1;
  }

  if (agora - ultimoTiro >= 650) {
    ultimoTiro = agora;
    dispararJogador();
  }

  if (agora - ultimoTiroInimigo >= 950) {
    ultimoTiroInimigo = agora;
    dispararInimigo();
  }

  if (tiroAtivo) {
    tiroY -= 4;
    if (tiroY < 0) tiroAtivo = false;
    verificarTiroJogador();
  }

  if (tiroInimigoAtivo) {
    tiroInimigoY += 3;

    if (tiroInimigoY >= PLAYER_Y - 4 &&
        abs(tiroInimigoX - jogadorX) <= 6) {
      iniciarExplosao();
    }

    if (tiroInimigoY >= SCREEN_HEIGHT) {
      tiroInimigoAtivo = false;
    }
  }

  if (!aindaHaInvasores()) {
    reiniciarInvasores();
  }

  if (grupoY > 38) {
    iniciarExplosao();
  }
}

void desenharCena() {
  display.clearDisplay();

  for (int r = 0; r < INV_ROWS; r++) {
    for (int c = 0; c < INV_COLS; c++) {
      if (invasorVivo[r][c]) {
        desenharInvader(
          grupoX + c * 20,
          grupoY + r * 12,
          frameInvasor
        );
      }
    }
  }

  if (explosaoJogador) {
    desenharExplosao(jogadorX, PLAYER_Y);
  } else {
    desenharCanhao(jogadorX, PLAYER_Y);
  }

  if (tiroAtivo) {
    display.drawFastVLine(tiroX, tiroY, 3, SSD1306_WHITE);
  }

  if (tiroInimigoAtivo) {
    display.drawFastVLine(tiroInimigoX, tiroInimigoY, 3, SSD1306_WHITE);
  }

  display.drawFastHLine(0, 63, SCREEN_WIDTH, SSD1306_WHITE);
  display.display();
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  randomSeed((unsigned long)analogRead(A0) ^ micros());
  reiniciarInvasores();
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    atualizarJogo();
    desenharCena();
  }
}

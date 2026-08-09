#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define TRILHO_Y 54
#define FRAME_INTERVAL 45
#define VELOCIDADE 2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int tremX = -72;
int direcao = 1; // 1 = direita, -1 = esquerda
byte faseRoda = 0;
byte faseFumaca = 0;

unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;

// ------------------------------------------------------
// CENARIO
// ------------------------------------------------------

void desenharTrilhos() {
  display.drawFastHLine(0, TRILHO_Y, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, TRILHO_Y + 5, SCREEN_WIDTH, SSD1306_WHITE);

  // Dormentes.
  for (int x = 0; x < SCREEN_WIDTH; x += 12) {
    display.drawLine(x, TRILHO_Y - 1, x + 5, TRILHO_Y + 7, SSD1306_WHITE);
  }
}

void desenharPaisagem() {
  // Montanhas simples ao fundo.
  display.drawLine(0, 28, 18, 14, SSD1306_WHITE);
  display.drawLine(18, 14, 35, 28, SSD1306_WHITE);

  display.drawLine(29, 28, 46, 18, SSD1306_WHITE);
  display.drawLine(46, 18, 61, 28, SSD1306_WHITE);

  // Sol/lua estilizado.
  display.drawCircle(111, 11, 5, SSD1306_WHITE);

  // Pequenos postes.
  display.drawFastVLine(10, 34, 16, SSD1306_WHITE);
  display.drawFastHLine(6, 36, 9, SSD1306_WHITE);

  display.drawFastVLine(118, 34, 16, SSD1306_WHITE);
  display.drawFastHLine(114, 36, 9, SSD1306_WHITE);
}

// ------------------------------------------------------
// RODAS E BIELA
// ------------------------------------------------------

void desenharRoda(int cx, int cy, byte fase) {
  display.drawCircle(cx, cy, 6, SSD1306_WHITE);
  display.drawCircle(cx, cy, 5, SSD1306_WHITE);
  display.fillCircle(cx, cy, 1, SSD1306_WHITE);

  switch (fase % 4) {
    case 0:
      display.drawLine(cx - 4, cy, cx + 4, cy, SSD1306_WHITE);
      display.drawLine(cx, cy - 4, cx, cy + 4, SSD1306_WHITE);
      break;

    case 1:
      display.drawLine(cx - 3, cy - 3, cx + 3, cy + 3, SSD1306_WHITE);
      display.drawLine(cx + 3, cy - 3, cx - 3, cy + 3, SSD1306_WHITE);
      break;

    case 2:
      display.drawLine(cx - 4, cy + 1, cx + 4, cy - 1, SSD1306_WHITE);
      display.drawLine(cx - 1, cy - 4, cx + 1, cy + 4, SSD1306_WHITE);
      break;

    default:
      display.drawLine(cx - 3, cy + 3, cx + 3, cy - 3, SSD1306_WHITE);
      display.drawLine(cx - 3, cy - 3, cx + 3, cy + 3, SSD1306_WHITE);
      break;
  }
}

void desenharBiela(int roda1X, int roda2X, int y, byte fase) {
  int deslocamento = 0;

  switch (fase % 4) {
    case 0: deslocamento = 0; break;
    case 1: deslocamento = -2; break;
    case 2: deslocamento = 0; break;
    default: deslocamento = 2; break;
  }

  display.drawLine(
    roda1X,
    y + deslocamento,
    roda2X,
    y - deslocamento,
    SSD1306_WHITE
  );
}

// ------------------------------------------------------
// FUMACA
// ------------------------------------------------------

void desenharFumaca(int chamineX, int topoY, int dir) {
  // A fumaca fica sempre atras do sentido de movimento.
  int lado = -dir;
  int desloca = faseFumaca % 3;

  display.drawCircle(
    chamineX + lado * (8 + desloca),
    topoY - 5 - desloca,
    2,
    SSD1306_WHITE
  );

  display.drawCircle(
    chamineX + lado * (14 + desloca),
    topoY - 9 - desloca,
    3,
    SSD1306_WHITE
  );

  display.drawCircle(
    chamineX + lado * (22 + desloca),
    topoY - 13 - desloca,
    4,
    SSD1306_WHITE
  );
}

// ------------------------------------------------------
// LOCOMOTIVA
// ------------------------------------------------------

void desenharLocomotiva(int x, int y, int dir) {
  int frente = dir > 0 ? x + 44 : x;
  int traseira = dir > 0 ? x : x + 44;

  // Base/chassi.
  display.fillRect(x + 4, y - 13, 40, 8, SSD1306_WHITE);

  // Caldeira.
  display.fillRoundRect(x + 15, y - 29, 27, 16, 7, SSD1306_WHITE);

  // Cabine.
  display.fillRect(x + 2, y - 32, 15, 19, SSD1306_WHITE);

  // Janela da cabine.
  display.fillRect(x + 5, y - 29, 7, 7, SSD1306_BLACK);

  // Teto.
  display.drawFastHLine(x, y - 33, 20, SSD1306_WHITE);
  display.drawFastHLine(x + 2, y - 34, 16, SSD1306_WHITE);

  // Frente da locomotiva.
  if (dir > 0) {
    display.fillRect(x + 41, y - 24, 5, 11, SSD1306_WHITE);
    display.drawLine(x + 44, y - 15, x + 50, y - 8, SSD1306_WHITE);
    display.drawLine(x + 50, y - 8, x + 44, y - 8, SSD1306_WHITE);
  } else {
    display.fillRect(x, y - 24, 5, 11, SSD1306_WHITE);
    display.drawLine(x + 2, y - 15, x - 6, y - 8, SSD1306_WHITE);
    display.drawLine(x - 6, y - 8, x + 1, y - 8, SSD1306_WHITE);
  }

  // Chamine posicionada na parte dianteira da caldeira.
  int chamineX = dir > 0 ? x + 36 : x + 10;

  display.fillRect(chamineX - 2, y - 38, 5, 10, SSD1306_WHITE);
  display.drawFastHLine(chamineX - 4, y - 39, 9, SSD1306_WHITE);
  display.drawFastHLine(chamineX - 3, y - 40, 7, SSD1306_WHITE);

  desenharFumaca(chamineX, y - 40, dir);

  // Farol frontal.
  int farolX = dir > 0 ? frente + 1 : frente - 1;
  display.fillCircle(farolX, y - 22, 2, SSD1306_WHITE);

  // Rodas.
  int roda1X = x + 12;
  int roda2X = x + 29;
  int roda3X = x + 41;
  int rodaY = y - 2;

  desenharRoda(roda1X, rodaY, faseRoda);
  desenharRoda(roda2X, rodaY, faseRoda);
  desenharRoda(roda3X, rodaY, faseRoda);

  desenharBiela(roda1X, roda3X, rodaY, faseRoda);

  // Engate traseiro.
  display.drawFastHLine(traseira + (dir > 0 ? -5 : 1), y - 9, 6, SSD1306_WHITE);
}

// ------------------------------------------------------
// VAGAO
// ------------------------------------------------------

void desenharVagao(int x, int y, int dir) {
  display.drawRect(x, y - 25, 30, 20, SSD1306_WHITE);
  display.drawFastHLine(x - 2, y - 26, 34, SSD1306_WHITE);

  // Ripas laterais.
  for (int i = 5; i < 30; i += 8) {
    display.drawFastVLine(x + i, y - 23, 16, SSD1306_WHITE);
  }

  // Rodas pequenas.
  desenharRoda(x + 7, y - 2, faseRoda);
  desenharRoda(x + 23, y - 2, faseRoda);

  // Engates.
  if (dir > 0) {
    display.drawFastHLine(x + 29, y - 10, 8, SSD1306_WHITE);
  } else {
    display.drawFastHLine(x - 7, y - 10, 8, SSD1306_WHITE);
  }
}

// ------------------------------------------------------
// TREM COMPLETO
// ------------------------------------------------------

void desenharTrem() {
  int locoX;
  int vagaoX;

  if (direcao > 0) {
    vagaoX = tremX;
    locoX = tremX + 34;
  } else {
    locoX = tremX;
    vagaoX = tremX + 50;
  }

  desenharVagao(vagaoX, TRILHO_Y, direcao);
  desenharLocomotiva(locoX, TRILHO_Y, direcao);
}

// ------------------------------------------------------
// ANIMACAO
// ------------------------------------------------------

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoPasso >= 110) {
    ultimoPasso = agora;
    faseRoda++;
    faseFumaca++;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) {
    return;
  }

  ultimoFrame = agora;

  tremX += direcao * VELOCIDADE;

  // Quando o trem sai da tela, volta pelo lado oposto.
  if (direcao > 0 && tremX > SCREEN_WIDTH + 20) {
    direcao = -1;
    tremX = SCREEN_WIDTH + 72;
  }
  else if (direcao < 0 && tremX < -90) {
    direcao = 1;
    tremX = -72;
  }

  display.clearDisplay();

  desenharPaisagem();
  desenharTrilhos();
  desenharTrem();

  display.display();
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  display.clearDisplay();
  display.display();

  Serial.println(F("Locomotiva iniciada"));
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------

void loop() {
  atualizarAnimacao();
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define Y_PERSONAGENS 32
#define DISTANCIA 30
#define VELOCIDADE 2
#define FRAME_INTERVAL 35

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------------------------------------------
// ESTADO DA ANIMACAO
// ------------------------------------------------------

int xPerseguidor = -22;
int direcao = 1; // 1 = direita, -1 = esquerda

bool pacmanPersegue = true;
bool bocaAberta = true;
bool passoFantasma = false;

unsigned long ultimoFrame = 0;
unsigned long ultimaBoca = 0;


// ------------------------------------------------------
// DESENHA O PAC-MAN
// ------------------------------------------------------

void desenharPacman(int x, int y, int dir, bool aberta) {
  const int raio = 10;

  // Corpo.
  display.fillCircle(x, y, raio, SSD1306_WHITE);

  // Boca.
  if (aberta) {
    if (dir > 0) {
      display.fillTriangle(
        x,
        y,
        x + raio + 2,
        y - 7,
        x + raio + 2,
        y + 7,
        SSD1306_BLACK
      );
    }
    else {
      display.fillTriangle(
        x,
        y,
        x - raio - 2,
        y - 7,
        x - raio - 2,
        y + 7,
        SSD1306_BLACK
      );
    }
  }

  // Olho.
  int olhoX = x + (dir > 0 ? 2 : -2);
  display.fillCircle(olhoX, y - 5, 1, SSD1306_BLACK);
}


// ------------------------------------------------------
// DESENHA O FANTASMA
// ------------------------------------------------------

void desenharFantasma(int x, int y, int dir, bool passo) {
  const int largura = 18;
  const int altura = 20;
  const int esquerda = x - largura / 2;
  const int topo = y - altura / 2;

  // Cabeca arredondada e corpo.
  display.fillRoundRect(
    esquerda,
    topo,
    largura,
    altura - 4,
    8,
    SSD1306_WHITE
  );

  display.fillRect(
    esquerda,
    y,
    largura,
    6,
    SSD1306_WHITE
  );

  // Base ondulada do fantasma.
  if (passo) {
    display.fillTriangle(esquerda, y + 5, esquerda + 4, y + 10, esquerda + 7, y + 5, SSD1306_WHITE);
    display.fillTriangle(esquerda + 6, y + 5, esquerda + 10, y + 10, esquerda + 13, y + 5, SSD1306_WHITE);
    display.fillTriangle(esquerda + 12, y + 5, esquerda + 16, y + 10, esquerda + 18, y + 5, SSD1306_WHITE);
  }
  else {
    display.fillTriangle(esquerda, y + 5, esquerda + 3, y + 9, esquerda + 6, y + 5, SSD1306_WHITE);
    display.fillTriangle(esquerda + 5, y + 5, esquerda + 9, y + 10, esquerda + 12, y + 5, SSD1306_WHITE);
    display.fillTriangle(esquerda + 11, y + 5, esquerda + 15, y + 9, esquerda + 18, y + 5, SSD1306_WHITE);
  }

  // Olhos brancos recortados no corpo.
  display.fillCircle(x - 4, y - 3, 3, SSD1306_BLACK);
  display.fillCircle(x + 4, y - 3, 3, SSD1306_BLACK);

  // Pupilas claras olhando na direcao do movimento.
  int deslocamento = dir > 0 ? 1 : -1;

  display.fillCircle(x - 4 + deslocamento, y - 3, 1, SSD1306_WHITE);
  display.fillCircle(x + 4 + deslocamento, y - 3, 1, SSD1306_WHITE);
}


// ------------------------------------------------------
// BOLINHAS DO CENARIO
// ------------------------------------------------------

void desenharBolinhas() {
  for (int x = 8; x < SCREEN_WIDTH; x += 16) {
    display.fillCircle(x, Y_PERSONAGENS, 1, SSD1306_WHITE);
  }
}


// ------------------------------------------------------
// TROCA O SENTIDO E QUEM PERSEGUE QUEM
// ------------------------------------------------------

void inverterPerseguicao() {
  direcao *= -1;
  pacmanPersegue = !pacmanPersegue;

  if (direcao > 0) {
    xPerseguidor = -22;
  }
  else {
    xPerseguidor = SCREEN_WIDTH + 22;
  }
}


// ------------------------------------------------------
// ATUALIZA A ANIMACAO
// ------------------------------------------------------

void atualizarAnimacao() {
  unsigned long agora = millis();

  // Abre e fecha a boca independentemente da velocidade do movimento.
  if (agora - ultimaBoca >= 120) {
    ultimaBoca = agora;
    bocaAberta = !bocaAberta;
    passoFantasma = !passoFantasma;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) {
    return;
  }

  ultimoFrame = agora;

  // O personagem perseguido fica na frente, na direcao do movimento.
  int xAlvo = xPerseguidor + (direcao * DISTANCIA);

  int xPacman;
  int xFantasma;

  if (pacmanPersegue) {
    xPacman = xPerseguidor;
    xFantasma = xAlvo;
  }
  else {
    xFantasma = xPerseguidor;
    xPacman = xAlvo;
  }

  display.clearDisplay();

  desenharBolinhas();

  desenharPacman(
    xPacman,
    Y_PERSONAGENS,
    direcao,
    bocaAberta
  );

  desenharFantasma(
    xFantasma,
    Y_PERSONAGENS,
    direcao,
    passoFantasma
  );

  display.display();

  // Move o grupo.
  xPerseguidor += direcao * VELOCIDADE;

  // Quando os dois personagens saem totalmente da tela,
  // inverte o sentido e troca quem esta perseguindo.
  if (direcao > 0 && xPerseguidor > SCREEN_WIDTH + 24) {
    inverterPerseguicao();
  }
  else if (direcao < 0 && xPerseguidor < -24) {
    inverterPerseguicao();
  }
}


// ------------------------------------------------------
// SETUP
// ------------------------------------------------------

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));

    for (;;) {
      // Para aqui caso o display nao inicialize.
    }
  }

  display.clearDisplay();
  display.display();

  Serial.println(F("Pac-Man iniciado"));
}


// ------------------------------------------------------
// LOOP
// ------------------------------------------------------

void loop() {
  atualizarAnimacao();
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define Y_CHAO 51
#define NUM_FORMIGAS 4
#define ESPACO_FORMIGAS 22
#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int xLider = -10;
int direcao = 1; // 1 = indo buscar comida, -1 = voltando para o formigueiro
bool carregando = false;
bool passo = false;

unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;

void desenharChao() {
  display.drawFastHLine(0, Y_CHAO + 3, SCREEN_WIDTH, SSD1306_WHITE);

  for (int x = 4; x < SCREEN_WIDTH; x += 13) {
    display.drawPixel(x, Y_CHAO + 6 + (x % 3), SSD1306_WHITE);
  }
}

void desenharFormigueiro() {
  // Pequeno monte e entrada escura do formigueiro.
  display.drawLine(0, Y_CHAO + 3, 5, Y_CHAO - 2, SSD1306_WHITE);
  display.drawLine(5, Y_CHAO - 2, 13, Y_CHAO + 3, SSD1306_WHITE);
  display.fillCircle(6, Y_CHAO + 1, 2, SSD1306_BLACK);
}

void desenharComida() {
  // Pilha de migalhas no lado direito.
  display.fillCircle(116, Y_CHAO, 3, SSD1306_WHITE);
  display.fillCircle(121, Y_CHAO + 1, 2, SSD1306_WHITE);
  display.fillCircle(111, Y_CHAO + 2, 2, SSD1306_WHITE);
  display.drawPixel(118, Y_CHAO - 5, SSD1306_WHITE);
  display.drawPixel(124, Y_CHAO - 2, SSD1306_WHITE);
}

void desenharCarga(int x, int y, int dir, int indice) {
  if (indice % 2 == 0) {
    // Folhinha em formato de losango.
    display.fillTriangle(x - 3, y - 7, x + 1, y - 11, x + 4, y - 6, SSD1306_WHITE);
    display.drawLine(x, y - 7, x + dir * 3, y - 4, SSD1306_WHITE);
  } else {
    // Migalha redonda.
    display.fillCircle(x, y - 8, 3, SSD1306_WHITE);
  }
}

void desenharFormiga(int x, int y, int dir, bool quadroPasso, bool temCarga, int indice) {
  // Corpo: abdomen, torax e cabeca.
  int cabecaX = x + dir * 5;
  int abdomenX = x - dir * 5;

  display.fillCircle(abdomenX, y, 3, SSD1306_WHITE);
  display.fillCircle(x, y, 2, SSD1306_WHITE);
  display.fillCircle(cabecaX, y, 2, SSD1306_WHITE);

  // Antenas.
  display.drawLine(cabecaX, y - 1, cabecaX + dir * 4, y - 5, SSD1306_WHITE);
  display.drawLine(cabecaX, y + 1, cabecaX + dir * 4, y + 3, SSD1306_WHITE);

  // Pernas alternam entre dois quadros para simular caminhada.
  if (quadroPasso) {
    display.drawLine(x - 2, y + 1, x - 6, y + 5, SSD1306_WHITE);
    display.drawLine(x, y + 1, x + 4, y + 5, SSD1306_WHITE);
    display.drawLine(x + 2, y + 1, x - 1, y + 5, SSD1306_WHITE);

    display.drawLine(x - 2, y - 1, x + 1, y - 4, SSD1306_WHITE);
    display.drawLine(x + 2, y - 1, x + 6, y - 4, SSD1306_WHITE);
  } else {
    display.drawLine(x - 2, y + 1, x + 1, y + 5, SSD1306_WHITE);
    display.drawLine(x, y + 1, x - 4, y + 5, SSD1306_WHITE);
    display.drawLine(x + 2, y + 1, x + 6, y + 5, SSD1306_WHITE);

    display.drawLine(x - 2, y - 1, x - 6, y - 4, SSD1306_WHITE);
    display.drawLine(x + 2, y - 1, x - 1, y - 4, SSD1306_WHITE);
  }

  if (temCarga) {
    desenharCarga(x, y, dir, indice);
  }
}

void inverterMarcha() {
  if (direcao > 0) {
    // Chegaram ao alimento e voltam carregadas.
    direcao = -1;
    carregando = true;
    xLider = SCREEN_WIDTH + 10;
    Serial.println(F("Formigas voltando com comida"));
  } else {
    // Entregaram a comida no formigueiro e saem novamente.
    direcao = 1;
    carregando = false;
    xLider = -10;
    Serial.println(F("Formigas indo buscar comida"));
  }
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoPasso >= 130) {
    ultimoPasso = agora;
    passo = !passo;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  xLider += direcao;

  int xUltima = xLider - direcao * ((NUM_FORMIGAS - 1) * ESPACO_FORMIGAS);

  if (direcao > 0 && xUltima > SCREEN_WIDTH + 10) {
    inverterMarcha();
  }
  else if (direcao < 0 && xUltima < -10) {
    inverterMarcha();
  }

  display.clearDisplay();

  desenharChao();
  desenharFormigueiro();
  desenharComida();

  for (int i = 0; i < NUM_FORMIGAS; i++) {
    int x = xLider - direcao * (i * ESPACO_FORMIGAS);
    int y = Y_CHAO - 3 + (i % 2);

    desenharFormiga(
      x,
      y,
      direcao,
      (i % 2 == 0) ? passo : !passo,
      carregando,
      i
    );
  }

  display.display();
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  display.clearDisplay();
  display.display();

  Serial.println(F("Formigas iniciadas"));
}

void loop() {
  atualizarAnimacao();
}

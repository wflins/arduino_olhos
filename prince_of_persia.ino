#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define CHAO_Y 54
#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int princeX = -10;
bool passo = false;
byte chama = 0;
unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;

int alturaSalto(int x) {
  if (x < 48 || x > 94) return 0;

  int centro = 71;
  int d = abs(x - centro);
  int h = 18 - (d * 18 / 23);
  if (h < 0) h = 0;
  return h;
}

void desenharCenario() {
  // Piso de pedra com um grande vao no meio.
  display.drawFastHLine(0, CHAO_Y, 53, SSD1306_WHITE);
  display.drawFastHLine(92, CHAO_Y, 36, SSD1306_WHITE);

  for (int x = 0; x < 53; x += 12) {
    display.drawFastHLine(x, CHAO_Y + 5, 9, SSD1306_WHITE);
  }

  for (int x = 94; x < 128; x += 12) {
    display.drawFastHLine(x, CHAO_Y + 5, 9, SSD1306_WHITE);
  }

  // Espinhos no fundo do vao.
  for (int x = 57; x < 91; x += 7) {
    display.drawTriangle(x, 62, x + 3, 55, x + 6, 62, SSD1306_WHITE);
  }

  // Porta no final da sala.
  display.drawRect(108, 26, 17, 28, SSD1306_WHITE);
  display.drawRoundRect(111, 30, 11, 24, 5, SSD1306_WHITE);
  display.fillCircle(119, 43, 1, SSD1306_WHITE);

  // Tocha.
  display.drawLine(18, 27, 18, 38, SSD1306_WHITE);
  display.drawLine(16, 38, 20, 38, SSD1306_WHITE);

  int topo = 24 - (chama % 2);
  display.fillTriangle(18, topo, 15, 30, 21, 30, SSD1306_WHITE);
  display.drawPixel(18 + (chama % 3) - 1, topo - 2, SSD1306_WHITE);
}

void desenharPrincipe(int x, int baseY, bool quadro) {
  // Cabeca e turbante/faixa.
  display.drawCircle(x, baseY - 23, 3, SSD1306_WHITE);
  display.drawLine(x - 4, baseY - 26, x + 4, baseY - 26, SSD1306_WHITE);

  // Corpo.
  display.drawLine(x, baseY - 20, x, baseY - 9, SSD1306_WHITE);
  display.drawLine(x - 3, baseY - 17, x + 3, baseY - 17, SSD1306_WHITE);

  // Bracos e pernas alternados para simular corrida.
  if (quadro) {
    display.drawLine(x, baseY - 17, x + 7, baseY - 13, SSD1306_WHITE);
    display.drawLine(x, baseY - 16, x - 6, baseY - 20, SSD1306_WHITE);

    display.drawLine(x, baseY - 9, x + 7, baseY - 3, SSD1306_WHITE);
    display.drawLine(x + 7, baseY - 3, x + 10, baseY, SSD1306_WHITE);
    display.drawLine(x, baseY - 9, x - 5, baseY - 2, SSD1306_WHITE);
  } else {
    display.drawLine(x, baseY - 17, x + 6, baseY - 21, SSD1306_WHITE);
    display.drawLine(x, baseY - 16, x - 7, baseY - 12, SSD1306_WHITE);

    display.drawLine(x, baseY - 9, x + 4, baseY - 1, SSD1306_WHITE);
    display.drawLine(x, baseY - 9, x - 7, baseY - 4, SSD1306_WHITE);
    display.drawLine(x - 7, baseY - 4, x - 10, baseY, SSD1306_WHITE);
  }
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoPasso >= 110) {
    ultimoPasso = agora;
    passo = !passo;
    chama++;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  princeX += 2;

  if (princeX > 132) {
    princeX = -10;
  }

  int salto = alturaSalto(princeX);

  display.clearDisplay();
  desenharCenario();
  desenharPrincipe(princeX, CHAO_Y - salto, passo);
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
}

void loop() {
  atualizarAnimacao();
}

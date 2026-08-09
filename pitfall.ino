#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define SOLO_Y 53
#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int heroiX = -10;
bool passo = false;
bool cipoFase = false;
unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;

int saltoPitfall(int x) {
  // Pula o tronco.
  if (x >= 24 && x <= 48) {
    int d = abs(x - 36);
    return 13 - (d * 13 / 12);
  }

  // Pula o buraco.
  if (x >= 72 && x <= 108) {
    int d = abs(x - 90);
    return 18 - (d * 18 / 18);
  }

  return 0;
}

void desenharJungle() {
  // Solo.
  display.drawFastHLine(0, SOLO_Y, 69, SSD1306_WHITE);
  display.drawFastHLine(109, SOLO_Y, 19, SSD1306_WHITE);

  // Buraco.
  display.drawLine(69, SOLO_Y, 74, 60, SSD1306_WHITE);
  display.drawLine(104, 60, 109, SOLO_Y, SSD1306_WHITE);
  display.drawFastHLine(74, 60, 30, SSD1306_WHITE);

  // Tronco rolando/obstaculo.
  display.drawCircle(37, SOLO_Y - 4, 5, SSD1306_WHITE);
  display.drawLine(33, SOLO_Y - 6, 41, SOLO_Y - 2, SSD1306_WHITE);
  display.drawLine(33, SOLO_Y - 2, 41, SOLO_Y - 6, SSD1306_WHITE);

  // Cipo balancando sobre o buraco.
  int pontaX = cipoFase ? 83 : 96;
  display.drawLine(90, 0, pontaX, 31, SSD1306_WHITE);
  display.fillCircle(pontaX, 31, 2, SSD1306_WHITE);

  // Folhagem superior.
  for (int x = 4; x < SCREEN_WIDTH; x += 18) {
    display.drawLine(x, 2, x + 5, 8, SSD1306_WHITE);
    display.drawLine(x + 5, 8, x + 10, 2, SSD1306_WHITE);
  }

  // Arbustos.
  display.drawCircle(10, SOLO_Y - 4, 4, SSD1306_WHITE);
  display.drawCircle(17, SOLO_Y - 3, 5, SSD1306_WHITE);
  display.drawCircle(118, SOLO_Y - 4, 5, SSD1306_WHITE);
}

void desenharHeroi(int x, int baseY, bool quadro) {
  // Cabeca.
  display.drawCircle(x, baseY - 20, 3, SSD1306_WHITE);

  // Corpo.
  display.drawLine(x, baseY - 17, x, baseY - 7, SSD1306_WHITE);

  // Bracos.
  if (quadro) {
    display.drawLine(x, baseY - 14, x + 6, baseY - 10, SSD1306_WHITE);
    display.drawLine(x, baseY - 13, x - 5, baseY - 17, SSD1306_WHITE);
  } else {
    display.drawLine(x, baseY - 14, x + 5, baseY - 18, SSD1306_WHITE);
    display.drawLine(x, baseY - 13, x - 6, baseY - 9, SSD1306_WHITE);
  }

  // Pernas.
  if (quadro) {
    display.drawLine(x, baseY - 7, x + 6, baseY - 1, SSD1306_WHITE);
    display.drawLine(x, baseY - 7, x - 5, baseY, SSD1306_WHITE);
  } else {
    display.drawLine(x, baseY - 7, x + 4, baseY, SSD1306_WHITE);
    display.drawLine(x, baseY - 7, x - 7, baseY - 2, SSD1306_WHITE);
  }
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoPasso >= 120) {
    ultimoPasso = agora;
    passo = !passo;
    cipoFase = !cipoFase;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  heroiX += 2;
  if (heroiX > 136) heroiX = -10;

  int salto = saltoPitfall(heroiX);

  display.clearDisplay();
  desenharJungle();
  desenharHeroi(heroiX, SOLO_Y - salto, passo);
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

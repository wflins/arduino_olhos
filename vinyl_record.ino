#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
#define FRAME_INTERVAL 60

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

byte faseDisco = 0;
int anguloBraco = 0;
int direcaoBraco = 1;
bool led = false;
unsigned long ultimoFrame = 0;
unsigned long ultimoLed = 0;
unsigned long ultimoBraco = 0;

const int8_t pontosX[8] = {0, 7, 10, 7, 0, -7, -10, -7};
const int8_t pontosY[8] = {-10, -7, 0, 7, 10, 7, 0, -7};

void desenharDisco() {
  const int cx = 42;
  const int cy = 32;

  display.fillCircle(cx, cy, 27, SSD1306_WHITE);
  display.fillCircle(cx, cy, 24, SSD1306_BLACK);
  display.drawCircle(cx, cy, 22, SSD1306_WHITE);
  display.drawCircle(cx, cy, 18, SSD1306_WHITE);
  display.drawCircle(cx, cy, 14, SSD1306_WHITE);

  // Label central.
  display.fillCircle(cx, cy, 8, SSD1306_WHITE);
  display.fillCircle(cx, cy, 2, SSD1306_BLACK);

  // Marca giratoria no label.
  int idx = faseDisco % 8;
  display.fillCircle(
    cx + pontosX[idx] / 2,
    cy + pontosY[idx] / 2,
    1,
    SSD1306_BLACK
  );

  // Reflexo girando na borda.
  display.drawPixel(
    cx + pontosX[idx] * 2,
    cy + pontosY[idx] * 2,
    SSD1306_WHITE
  );
}

void desenharTocaDiscos() {
  // Base.
  display.drawRoundRect(2, 2, 124, 60, 5, SSD1306_WHITE);

  // Painel direito.
  display.drawRect(82, 7, 38, 49, SSD1306_WHITE);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(89, 10);
  display.print(F("33 RPM"));

  // Botao e LED.
  display.drawCircle(91, 49, 4, SSD1306_WHITE);
  display.fillCircle(112, 49, led ? 2 : 1, SSD1306_WHITE);
}

void desenharBraco() {
  // Pivo do braco.
  const int px = 108;
  const int py = 28;

  display.fillCircle(px, py, 4, SSD1306_WHITE);
  display.fillCircle(px, py, 1, SSD1306_BLACK);

  // Pequeno movimento de ida e volta sobre o disco.
  int pontaX = 79 - anguloBraco;
  int pontaY = 22 + anguloBraco / 2;

  display.drawLine(px - 2, py, pontaX, pontaY, SSD1306_WHITE);
  display.fillRect(pontaX - 3, pontaY - 2, 6, 4, SSD1306_WHITE);
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoLed >= 450) {
    ultimoLed = agora;
    led = !led;
  }

  if (agora - ultimoBraco >= 500) {
    ultimoBraco = agora;
    anguloBraco += direcaoBraco;

    if (anguloBraco >= 8 || anguloBraco <= 0) {
      direcaoBraco *= -1;
    }
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  faseDisco++;

  display.clearDisplay();
  desenharTocaDiscos();
  desenharDisco();
  desenharBraco();
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

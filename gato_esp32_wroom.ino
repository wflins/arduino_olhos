#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_random.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// ESP32 WROOM-32 / DevKit
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000

#define FRAME_INTERVAL 45

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimoFrame = 0;
unsigned long ultimaPiscada = 0;
unsigned long proximaPiscada = 2500;
unsigned long inicioPiscada = 0;
unsigned long ultimaOrelha = 0;
unsigned long ultimaPata = 0;

bool piscando = false;
bool mexerOrelha = false;
bool levantarPata = false;

int caudaFase = 0;
int respiracaoFase = 0;

void desenharCabeca(int yOffset) {
  // Cabeca
  display.fillRoundRect(42, 14 + yOffset, 44, 31, 10, SSD1306_WHITE);

  // Orelhas
  int orelhaExtra = mexerOrelha ? 2 : 0;
  display.fillTriangle(45, 18 + yOffset,
                       49, 5 + yOffset + orelhaExtra,
                       57, 17 + yOffset,
                       SSD1306_WHITE);

  display.fillTriangle(71, 17 + yOffset,
                       79, 5 + yOffset,
                       83, 18 + yOffset,
                       SSD1306_WHITE);

  // Interior das orelhas
  display.fillTriangle(49, 16 + yOffset,
                       51, 10 + yOffset + orelhaExtra,
                       55, 16 + yOffset,
                       SSD1306_BLACK);

  display.fillTriangle(73, 16 + yOffset,
                       77, 10 + yOffset,
                       79, 16 + yOffset,
                       SSD1306_BLACK);

  // Olhos
  if (piscando) {
    display.drawFastHLine(51, 28 + yOffset, 8, SSD1306_BLACK);
    display.drawFastHLine(69, 28 + yOffset, 8, SSD1306_BLACK);
  } else {
    display.fillRoundRect(51, 24 + yOffset, 8, 9, 3, SSD1306_BLACK);
    display.fillRoundRect(69, 24 + yOffset, 8, 9, 3, SSD1306_BLACK);

    display.drawPixel(53, 26 + yOffset, SSD1306_WHITE);
    display.drawPixel(71, 26 + yOffset, SSD1306_WHITE);
  }

  // Focinho e nariz
  display.fillTriangle(62, 32 + yOffset,
                       66, 32 + yOffset,
                       64, 35 + yOffset,
                       SSD1306_BLACK);

  display.drawLine(64, 35 + yOffset, 64, 38 + yOffset, SSD1306_BLACK);
  display.drawLine(64, 38 + yOffset, 60, 40 + yOffset, SSD1306_BLACK);
  display.drawLine(64, 38 + yOffset, 68, 40 + yOffset, SSD1306_BLACK);

  // Bigodes
  display.drawLine(57, 35 + yOffset, 39, 32 + yOffset, SSD1306_BLACK);
  display.drawLine(57, 38 + yOffset, 37, 38 + yOffset, SSD1306_BLACK);
  display.drawLine(71, 35 + yOffset, 89, 32 + yOffset, SSD1306_BLACK);
  display.drawLine(71, 38 + yOffset, 91, 38 + yOffset, SSD1306_BLACK);
}

void desenharCorpo(int yOffset) {
  // Corpo sentado
  display.fillRoundRect(48, 40 + yOffset, 32, 22, 10, SSD1306_WHITE);

  // Peito
  display.fillRoundRect(57, 43 + yOffset, 14, 18, 7, SSD1306_BLACK);

  // Pernas
  display.fillRoundRect(44, 54 + yOffset, 17, 8, 4, SSD1306_WHITE);
  display.fillRoundRect(67, 54 + yOffset, 17, 8, 4, SSD1306_WHITE);

  // Pata que levanta ocasionalmente
  if (levantarPata) {
    display.fillRoundRect(39, 40 + yOffset, 9, 18, 4, SSD1306_WHITE);
    display.drawPixel(41, 41 + yOffset, SSD1306_BLACK);
    display.drawPixel(44, 40 + yOffset, SSD1306_BLACK);
    display.drawPixel(46, 42 + yOffset, SSD1306_BLACK);
  } else {
    display.fillRoundRect(43, 47 + yOffset, 9, 14, 4, SSD1306_WHITE);
  }
}

void desenharCauda(int yOffset) {
  // Cauda balancando atras do corpo.
  int desloc = 0;
  switch (caudaFase) {
    case 0: desloc = -5; break;
    case 1: desloc = -2; break;
    case 2: desloc = 2;  break;
    case 3: desloc = 5;  break;
    case 4: desloc = 2;  break;
    case 5: desloc = -2; break;
  }

  display.drawLine(80, 51 + yOffset, 91 + desloc, 48 + yOffset, SSD1306_WHITE);
  display.drawLine(91 + desloc, 48 + yOffset, 103 + desloc, 52 + yOffset, SSD1306_WHITE);
  display.drawLine(103 + desloc, 52 + yOffset, 111 + desloc, 47 + yOffset, SSD1306_WHITE);
  display.drawLine(80, 52 + yOffset, 91 + desloc, 50 + yOffset, SSD1306_WHITE);
}

void desenharChao() {
  display.drawFastHLine(18, 63, 92, SSD1306_WHITE);
}

void atualizarEventos() {
  unsigned long agora = millis();

  if (!piscando && agora - ultimaPiscada >= proximaPiscada) {
    piscando = true;
    inicioPiscada = agora;
  }

  if (piscando && agora - inicioPiscada >= 150) {
    piscando = false;
    ultimaPiscada = agora;
    proximaPiscada = random(1800, 4800);
  }

  if (agora - ultimaOrelha >= 3200) {
    ultimaOrelha = agora;
    mexerOrelha = true;
  }

  if (mexerOrelha && agora - ultimaOrelha >= 180) {
    mexerOrelha = false;
  }

  if (agora - ultimaPata >= 6500) {
    ultimaPata = agora;
    levantarPata = true;
  }

  if (levantarPata && agora - ultimaPata >= 900) {
    levantarPata = false;
  }
}

void desenharFrame() {
  respiracaoFase = (respiracaoFase + 1) % 20;
  int yOffset = (respiracaoFase >= 10) ? 1 : 0;

  caudaFase = (caudaFase + 1) % 6;

  display.clearDisplay();

  desenharCauda(yOffset);
  desenharCorpo(yOffset);
  desenharCabeca(yOffset);
  desenharChao();

  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS,
        true,
        false
      )) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {
      delay(1000);
    }
  }

  randomSeed(esp_random());

  display.clearDisplay();
  display.display();

  Serial.println(F("Gato ESP32 WROOM-32 iniciado"));
  Serial.print(F("OLED SDA GPIO"));
  Serial.print(OLED_SDA);
  Serial.print(F(" / SCL GPIO"));
  Serial.println(OLED_SCL);
}

void loop() {
  unsigned long agora = millis();

  atualizarEventos();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    desenharFrame();
  }
}

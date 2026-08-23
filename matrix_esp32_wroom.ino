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

#define FRAME_INTERVAL 55
#define COL_WIDTH 6
#define NUM_COLS (SCREEN_WIDTH / COL_WIDTH)
#define CHAR_HEIGHT 8
#define NUM_ROWS (SCREEN_HEIGHT / CHAR_HEIGHT)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct MatrixColumn {
  int headRow;
  int speedDiv;
  int tick;
  int length;
  bool active;
};

MatrixColumn columns[NUM_COLS];
unsigned long lastFrame = 0;

char randomGlyph() {
  static const char glyphs[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "#$%&*+-<>[]{}?/\\|";

  return glyphs[random(0, sizeof(glyphs) - 1)];
}

void resetColumn(int i, bool startAbove) {
  columns[i].headRow = startAbove ? -random(1, NUM_ROWS + 5) : -1;
  columns[i].speedDiv = random(1, 4);
  columns[i].tick = 0;
  columns[i].length = random(3, 8);
  columns[i].active = random(0, 100) < 88;
}

void initMatrix() {
  for (int i = 0; i < NUM_COLS; i++) {
    resetColumn(i, true);
  }
}

void drawDimGlyph(int x, int y, char c, int age) {
  // OLED monocromatico nao possui brilho real por pixel.
  // Simulamos intensidade usando padroes de pixels e omissoes.
  if (age <= 1) {
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x, y);
    display.write(c);
    return;
  }

  if (age == 2) {
    if (((x + y + millis() / 120) & 1) == 0) {
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(x, y);
      display.write(c);
    }
    return;
  }

  if (age == 3) {
    if (((x / COL_WIDTH) + (y / CHAR_HEIGHT)) % 2 == 0) {
      display.drawPixel(x + 1, y + 2, SSD1306_WHITE);
      display.drawPixel(x + 3, y + 5, SSD1306_WHITE);
    }
  }
}

void drawColumn(int i) {
  if (!columns[i].active) return;

  int x = i * COL_WIDTH;
  int head = columns[i].headRow;
  int len = columns[i].length;

  for (int age = 0; age < len; age++) {
    int row = head - age;
    if (row < 0 || row >= NUM_ROWS) continue;

    int y = row * CHAR_HEIGHT;
    char c = randomGlyph();

    if (age == 0) {
      // Cabeca da trilha: caractere mais marcante.
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(x, y);
      display.write(c);
      display.drawPixel(x + 4, y + 1, SSD1306_WHITE);
    } else {
      drawDimGlyph(x, y, c, age);
    }
  }
}

void updateColumns() {
  for (int i = 0; i < NUM_COLS; i++) {
    if (!columns[i].active) {
      if (random(0, 1000) < 8) {
        resetColumn(i, false);
        columns[i].active = true;
      }
      continue;
    }

    columns[i].tick++;
    if (columns[i].tick >= columns[i].speedDiv) {
      columns[i].tick = 0;
      columns[i].headRow++;
    }

    if (columns[i].headRow - columns[i].length > NUM_ROWS) {
      resetColumn(i, true);
    }

    // Pequena chance de alterar o comprimento durante a queda.
    if (random(0, 1000) < 5) {
      columns[i].length = constrain(columns[i].length + random(-1, 2), 3, 8);
    }
  }
}

void drawMatrix() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);

  for (int i = 0; i < NUM_COLS; i++) {
    drawColumn(i);
  }

  // Pequenos pontos aleatorios reforcam o efeito de "chuva digital".
  for (int i = 0; i < 8; i++) {
    if (random(0, 100) < 45) {
      display.drawPixel(random(0, SCREEN_WIDTH), random(0, SCREEN_HEIGHT), SSD1306_WHITE);
    }
  }

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
  display.setTextColor(SSD1306_WHITE);
  display.display();

  initMatrix();

  Serial.println(F("Matrix ESP32 WROOM-32 iniciado"));
  Serial.print(F("OLED SDA GPIO"));
  Serial.print(OLED_SDA);
  Serial.print(F(" / SCL GPIO"));
  Serial.println(OLED_SCL);
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame >= FRAME_INTERVAL) {
    lastFrame = now;
    updateColumns();
    drawMatrix();
  }
}

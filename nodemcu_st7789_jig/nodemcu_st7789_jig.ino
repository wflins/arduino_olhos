/*
  Jig de teste - NodeMCU ESP8266 + display IPS SPI ST7789

  Bibliotecas (Arduino Library Manager):
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library

  Ligacoes sugeridas (NodeMCU ESP8266):

    ST7789   NodeMCU
    ---------------------
    VCC   -> 3V3
    GND   -> GND
    SCL   -> D5 / GPIO14 (SCLK)
    SDA   -> D7 / GPIO13 (MOSI)
    RES   -> D1 / GPIO5
    DC    -> D2 / GPIO4
    CS    -> D8 / GPIO15  (se o display possuir CS)
    BLK   -> 3V3           (backlight sempre ligado)

  Observacoes:
    - Alguns modulos ST7789 nao possuem pino CS. Nesse caso use TFT_CS = -1.
    - O ST7789 normalmente nao usa MISO para escrita no display.
    - O NodeMCU trabalha com logica de 3,3 V.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// -----------------------------------------------------------------------------
// CONFIGURACAO DO DISPLAY
// -----------------------------------------------------------------------------

// Para display 240x240:
#define TFT_WIDTH   240
#define TFT_HEIGHT  240

// Para um ST7789 240x320, altere TFT_HEIGHT para 320.

// Pinos NodeMCU / ESP8266
#define TFT_CS   D8   // GPIO15. Se seu modulo nao possui CS, troque por -1
#define TFT_DC   D2   // GPIO4
#define TFT_RST  D1   // GPIO5

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// -----------------------------------------------------------------------------
// AUXILIARES
// -----------------------------------------------------------------------------

void centralText(const char *text, int16_t y, uint16_t color, uint8_t size) {
  int16_t x1, y1;
  uint16_t w, h;

  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.print(text);
}

void title(const char *text) {
  tft.fillScreen(ST77XX_BLACK);
  centralText(text, 8, ST77XX_WHITE, 2);
  delay(500);
}

void testSolidColors() {
  const uint16_t colors[] = {
    ST77XX_RED,
    ST77XX_GREEN,
    ST77XX_BLUE,
    ST77XX_WHITE,
    ST77XX_BLACK
  };

  const char *names[] = {
    "VERMELHO",
    "VERDE",
    "AZUL",
    "BRANCO",
    "PRETO"
  };

  for (uint8_t i = 0; i < 5; i++) {
    tft.fillScreen(colors[i]);

    if (colors[i] != ST77XX_BLACK) {
      centralText(names[i], tft.height() / 2 - 8, ST77XX_BLACK, 2);
    } else {
      centralText(names[i], tft.height() / 2 - 8, ST77XX_WHITE, 2);
    }

    delay(900);
  }
}

void testRGBBars() {
  title("TESTE RGB");

  int16_t y0 = 40;
  int16_t h = (tft.height() - y0) / 3;

  tft.fillRect(0, y0, tft.width(), h, ST77XX_RED);
  tft.fillRect(0, y0 + h, tft.width(), h, ST77XX_GREEN);
  tft.fillRect(0, y0 + (2 * h), tft.width(), tft.height() - (y0 + 2 * h), ST77XX_BLUE);

  delay(1600);
}

void testGeometry() {
  title("GEOMETRIA");

  int16_t cx = tft.width() / 2;
  int16_t cy = tft.height() / 2;

  tft.drawRect(1, 1, tft.width() - 2, tft.height() - 2, ST77XX_WHITE);
  tft.drawLine(0, 0, tft.width() - 1, tft.height() - 1, ST77XX_RED);
  tft.drawLine(tft.width() - 1, 0, 0, tft.height() - 1, ST77XX_GREEN);
  tft.drawCircle(cx, cy, min(tft.width(), tft.height()) / 4, ST77XX_CYAN);
  tft.fillCircle(cx, cy, 10, ST77XX_YELLOW);

  delay(1600);
}

void testText() {
  title("TEXTO");

  tft.setTextWrap(false);

  tft.setCursor(10, 45);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("NodeMCU ESP8266");
  tft.println("ST7789 SPI");

  tft.setCursor(10, 80);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("JIG TEST");

  tft.setCursor(10, 115);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print(tft.width());
  tft.print("x");
  tft.println(tft.height());

  tft.setCursor(10, 150);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("SPI: OK se esta tela aparece");

  delay(2200);
}

void testRefresh() {
  title("REFRESH");

  const int box = 28;
  const int y = tft.height() / 2;

  for (int x = 0; x < tft.width() - box; x += 4) {
    tft.fillRect(0, y - box / 2, tft.width(), box, ST77XX_BLACK);
    tft.fillRect(x, y - box / 2, box, box, ST77XX_MAGENTA);
    delay(12);
  }

  delay(500);
}

void testCorners() {
  title("BORDAS");

  const int s = 24;

  tft.fillRect(0, 0, s, s, ST77XX_RED);
  tft.fillRect(tft.width() - s, 0, s, s, ST77XX_GREEN);
  tft.fillRect(0, tft.height() - s, s, s, ST77XX_BLUE);
  tft.fillRect(tft.width() - s, tft.height() - s, s, s, ST77XX_YELLOW);

  centralText("4 CANTOS VISIVEIS?", tft.height() / 2 - 8, ST77XX_WHITE, 1);

  delay(1800);
}

void showPassScreen() {
  tft.fillScreen(ST77XX_GREEN);
  centralText("ST7789", tft.height() / 2 - 35, ST77XX_BLACK, 3);
  centralText("TESTE OK", tft.height() / 2 + 5, ST77XX_BLACK, 2);
  delay(2200);
}

// -----------------------------------------------------------------------------
// SETUP / LOOP
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("================================");
  Serial.println("JIG NodeMCU + ST7789");
  Serial.println("================================");

  // Inicializa SPI de hardware do ESP8266:
  // SCLK = D5 / GPIO14
  // MOSI = D7 / GPIO13
  SPI.begin();

  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setRotation(0);
  tft.setTextWrap(false);

  Serial.printf("Display configurado: %dx%d\n", TFT_WIDTH, TFT_HEIGHT);

  tft.fillScreen(ST77XX_BLACK);
  centralText("NODEMCU", 70, ST77XX_CYAN, 3);
  centralText("ST7789 JIG", 115, ST77XX_WHITE, 2);
  centralText("INICIANDO...", 155, ST77XX_YELLOW, 1);
  delay(1600);
}

void loop() {
  Serial.println("Teste: cores solidas");
  testSolidColors();

  Serial.println("Teste: barras RGB");
  testRGBBars();

  Serial.println("Teste: geometria");
  testGeometry();

  Serial.println("Teste: texto");
  testText();

  Serial.println("Teste: bordas");
  testCorners();

  Serial.println("Teste: refresh");
  testRefresh();

  Serial.println("Ciclo concluido");
  showPassScreen();
}

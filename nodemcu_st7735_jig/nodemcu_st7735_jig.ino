/*
  Jig de teste - NodeMCU ESP8266 + TFT SPI ST7735 128x160

  Bibliotecas (Arduino Library Manager):
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library

  Ligacoes sugeridas:

    ST7735   NodeMCU
    ----------------------
    VCC   -> 3V3
    GND   -> GND
    SCL   -> D5 / GPIO14 (SCK)
    SDA   -> D7 / GPIO13 (MOSI)
    RES   -> D1 / GPIO5
    A0/DC -> D2 / GPIO4
    CS    -> D8 / GPIO15
    LED   -> 3V3

  Observacao:
    A0 neste tipo de modulo e o mesmo sinal DC (Data/Command).
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   D8
#define TFT_DC   D2
#define TFT_RST  D1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void centerText(const char *text, int16_t y, uint16_t color, uint8_t size) {
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
  centerText(text, 6, ST77XX_WHITE, 1);
  delay(400);
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
    centerText(names[i], tft.height() / 2 - 4,
               colors[i] == ST77XX_BLACK ? ST77XX_WHITE : ST77XX_BLACK, 1);
    delay(800);
  }
}

void testRGBBars() {
  title("RGB");

  int16_t y0 = 24;
  int16_t h = (tft.height() - y0) / 3;

  tft.fillRect(0, y0, tft.width(), h, ST77XX_RED);
  tft.fillRect(0, y0 + h, tft.width(), h, ST77XX_GREEN);
  tft.fillRect(0, y0 + 2 * h, tft.width(), tft.height() - (y0 + 2 * h), ST77XX_BLUE);

  delay(1400);
}

void testGeometry() {
  title("GEOMETRIA");

  int16_t cx = tft.width() / 2;
  int16_t cy = tft.height() / 2;

  tft.drawRect(1, 1, tft.width() - 2, tft.height() - 2, ST77XX_WHITE);
  tft.drawLine(0, 0, tft.width() - 1, tft.height() - 1, ST77XX_RED);
  tft.drawLine(tft.width() - 1, 0, 0, tft.height() - 1, ST77XX_GREEN);
  tft.drawCircle(cx, cy, 35, ST77XX_CYAN);
  tft.fillCircle(cx, cy, 7, ST77XX_YELLOW);

  delay(1600);
}

void testText() {
  title("TEXTO");

  tft.setTextWrap(false);

  tft.setCursor(6, 28);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("NodeMCU ESP8266");
  tft.println("ST7735 SPI");

  tft.setCursor(6, 58);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("JIG");

  tft.setCursor(6, 88);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Resolucao: ");
  tft.print(tft.width());
  tft.print("x");
  tft.println(tft.height());

  tft.setCursor(6, 112);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("SPI OK se legivel");

  delay(2000);
}

void testCorners() {
  title("BORDAS");

  const int s = 16;
  tft.fillRect(0, 0, s, s, ST77XX_RED);
  tft.fillRect(tft.width() - s, 0, s, s, ST77XX_GREEN);
  tft.fillRect(0, tft.height() - s, s, s, ST77XX_BLUE);
  tft.fillRect(tft.width() - s, tft.height() - s, s, s, ST77XX_YELLOW);

  centerText("4 CANTOS?", tft.height() / 2 - 4, ST77XX_WHITE, 1);
  delay(1600);
}

void testRefresh() {
  title("REFRESH");

  const int box = 20;
  int y = tft.height() / 2 - box / 2;

  for (int x = 0; x < tft.width() - box; x += 3) {
    tft.fillRect(0, y, tft.width(), box, ST77XX_BLACK);
    tft.fillRect(x, y, box, box, ST77XX_MAGENTA);
    delay(12);
  }

  delay(500);
}

void showPassScreen() {
  tft.fillScreen(ST77XX_GREEN);
  centerText("ST7735", 50, ST77XX_BLACK, 2);
  centerText("TESTE OK", 82, ST77XX_BLACK, 2);
  delay(2200);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("===============================");
  Serial.println("JIG NodeMCU + ST7735 128x160");
  Serial.println("===============================");

  SPI.begin();

  // A variante BLACKTAB e a mais comum para os modulos 1.8 polegadas 128x160.
  // Se houver deslocamento de imagem ou cores estranhas, teste:
  // INITR_GREENTAB ou INITR_REDTAB.
  tft.initR(INITR_BLACKTAB);

  tft.setRotation(0);
  tft.setTextWrap(false);

  Serial.printf("Display configurado: %dx%d\n", tft.width(), tft.height());

  tft.fillScreen(ST77XX_BLACK);
  centerText("NODEMCU", 45, ST77XX_CYAN, 2);
  centerText("ST7735 JIG", 72, ST77XX_WHITE, 1);
  centerText("INICIANDO...", 98, ST77XX_YELLOW, 1);
  delay(1500);
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

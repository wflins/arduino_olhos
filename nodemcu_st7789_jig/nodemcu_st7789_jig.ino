/*
  Jig de teste - NodeMCU ESP8266 + display IPS SPI ST7789 1.54" 240x240

  Ligacoes:

    ST7789   NodeMCU
    ---------------------
    BL    -> 3V3
    CS    -> D8 / GPIO15
    DC    -> D2 / GPIO4
    RST   -> D1 / GPIO5
    SDA   -> D7 / GPIO13 (MOSI)
    SCL   -> D5 / GPIO14 (SCLK)
    VCC   -> 3V3
    GND   -> GND
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define TFT_WIDTH   240
#define TFT_HEIGHT  240

#define TFT_CS   D8
#define TFT_DC   D2
#define TFT_RST  D1

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

  int16_t x = (tft.width() - w) / 2 - x1;

  tft.setCursor(x, y);
  tft.print(text);
}

void title(const char *text) {
  tft.fillScreen(ST77XX_BLACK);
  centralText(text, 8, ST77XX_WHITE, 2);
  delay(400);
}

// -----------------------------------------------------------------------------
// TESTES
// -----------------------------------------------------------------------------

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

    if (colors[i] == ST77XX_BLACK) {
      centralText(names[i], 110, ST77XX_WHITE, 2);
    } else {
      centralText(names[i], 110, ST77XX_BLACK, 2);
    }

    delay(1000);
  }
}

void testRGBBars() {
  title("RGB");

  int16_t y0 = 40;
  int16_t h = (tft.height() - y0) / 3;

  tft.fillRect(0, y0, tft.width(), h, ST77XX_RED);
  tft.fillRect(0, y0 + h, tft.width(), h, ST77XX_GREEN);
  tft.fillRect(
    0,
    y0 + (2 * h),
    tft.width(),
    tft.height() - (y0 + 2 * h),
    ST77XX_BLUE
  );

  delay(1500);
}

void testGeometry() {
  tft.fillScreen(ST77XX_BLACK);

  centralText("GEOMETRIA", 8, ST77XX_WHITE, 2);

  int16_t cx = tft.width() / 2;
  int16_t cy = tft.height() / 2;

  tft.drawRect(
    1,
    1,
    tft.width() - 2,
    tft.height() - 2,
    ST77XX_WHITE
  );

  tft.drawLine(
    0,
    0,
    tft.width() - 1,
    tft.height() - 1,
    ST77XX_RED
  );

  tft.drawLine(
    tft.width() - 1,
    0,
    0,
    tft.height() - 1,
    ST77XX_GREEN
  );

  tft.drawCircle(
    cx,
    cy,
    55,
    ST77XX_CYAN
  );

  tft.fillCircle(
    cx,
    cy,
    10,
    ST77XX_YELLOW
  );

  delay(1500);
}

void testText() {
  tft.fillScreen(ST77XX_BLACK);

  centralText("TEXTO", 8, ST77XX_WHITE, 2);

  tft.setTextWrap(false);

  tft.setCursor(10, 50);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("NodeMCU ESP8266");
  tft.println("ST7789 SPI 240x240");

  tft.setCursor(10, 90);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("JIG TEST");

  tft.setCursor(10, 130);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print(tft.width());
  tft.print("x");
  tft.println(tft.height());

  tft.setCursor(10, 180);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("SPI OK");
  tft.println("PIXELS / TEXTO OK");

  delay(2000);
}

void testCorners() {
  tft.fillScreen(ST77XX_BLACK);

  const int s = 30;

  tft.fillRect(
    0,
    0,
    s,
    s,
    ST77XX_RED
  );

  tft.fillRect(
    tft.width() - s,
    0,
    s,
    s,
    ST77XX_GREEN
  );

  tft.fillRect(
    0,
    tft.height() - s,
    s,
    s,
    ST77XX_BLUE
  );

  tft.fillRect(
    tft.width() - s,
    tft.height() - s,
    s,
    s,
    ST77XX_YELLOW
  );

  centralText(
    "4 CANTOS",
    100,
    ST77XX_WHITE,
    2
  );

  centralText(
    "DEVEM APARECER",
    130,
    ST77XX_WHITE,
    1
  );

  delay(1800);
}

void testRefresh() {
  tft.fillScreen(ST77XX_BLACK);

  centralText("REFRESH", 8, ST77XX_WHITE, 2);

  const int box = 35;
  const int y = 125;

  for (int x = 0; x <= tft.width() - box; x += 4) {

    tft.fillRect(
      0,
      y - box / 2,
      tft.width(),
      box,
      ST77XX_BLACK
    );

    tft.fillRect(
      x,
      y - box / 2,
      box,
      box,
      ST77XX_MAGENTA
    );

    delay(12);
  }

  delay(400);
}

void testPixelGrid() {
  tft.fillScreen(ST77XX_BLACK);

  centralText("GRADE", 8, ST77XX_WHITE, 2);

  for (int x = 0; x < tft.width(); x += 20) {
    tft.drawLine(
      x,
      35,
      x,
      tft.height() - 1,
      ST77XX_CYAN
    );
  }

  for (int y = 35; y < tft.height(); y += 20) {
    tft.drawLine(
      0,
      y,
      tft.width() - 1,
      y,
      ST77XX_YELLOW
    );
  }

  delay(1600);
}

void showCycleEnd() {
  tft.fillScreen(ST77XX_GREEN);

  centralText(
    "ST7789",
    75,
    ST77XX_BLACK,
    3
  );

  centralText(
    "CICLO OK",
    125,
    ST77XX_BLACK,
    2
  );

  centralText(
    "REINICIANDO...",
    170,
    ST77XX_BLACK,
    1
  );

  delay(2000);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------

void setup() {

  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("==============================");
  Serial.println("JIG ST7789 240x240");
  Serial.println("NodeMCU ESP8266");
  Serial.println("==============================");

  SPI.begin();

  tft.init(
    TFT_WIDTH,
    TFT_HEIGHT
  );

  tft.setRotation(0);
  tft.setTextWrap(false);

  Serial.println("Display inicializado.");

  tft.fillScreen(ST77XX_BLACK);

  centralText(
    "NODEMCU",
    70,
    ST77XX_CYAN,
    3
  );

  centralText(
    "ST7789 JIG",
    115,
    ST77XX_WHITE,
    2
  );

  centralText(
    "240x240",
    150,
    ST77XX_YELLOW,
    2
  );

  centralText(
    "INICIANDO...",
    190,
    ST77XX_GREEN,
    1
  );

  delay(1800);
}

// -----------------------------------------------------------------------------
// LOOP CONTINUO
// -----------------------------------------------------------------------------

void loop() {

  Serial.println();
  Serial.println("===== NOVO CICLO =====");

  Serial.println("1 - Cores solidas");
  testSolidColors();

  Serial.println("2 - Barras RGB");
  testRGBBars();

  Serial.println("3 - Geometria");
  testGeometry();

  Serial.println("4 - Texto");
  testText();

  Serial.println("5 - Bordas");
  testCorners();

  Serial.println("6 - Grade");
  testPixelGrid();

  Serial.println("7 - Refresh");
  testRefresh();

  Serial.println("Ciclo concluido.");
  showCycleEnd();
}

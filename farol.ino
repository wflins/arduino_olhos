#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define MAR_Y 48
#define FAROL_X 96
#define LUZ_X 95
#define LUZ_Y 19
#define FRAME_INTERVAL 50

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int feixeY = 8;
int direcaoFeixe = 1;
int barcoX = -18;
int direcaoBarco = 1;

bool onda = false;
bool brilho = false;

unsigned long ultimoFrame = 0;
unsigned long ultimoFeixe = 0;
unsigned long ultimaOnda = 0;
unsigned long ultimoBrilho = 0;

void desenharEstrelas() {
  display.drawPixel(8, 8, SSD1306_WHITE);
  display.drawPixel(19, 15, SSD1306_WHITE);
  display.drawPixel(34, 6, SSD1306_WHITE);
  display.drawPixel(47, 17, SSD1306_WHITE);
  display.drawPixel(63, 9, SSD1306_WHITE);
  display.drawPixel(76, 14, SSD1306_WHITE);
  display.drawPixel(112, 8, SSD1306_WHITE);
  display.drawPixel(121, 18, SSD1306_WHITE);

  if (brilho) {
    display.drawLine(28, 22, 28, 26, SSD1306_WHITE);
    display.drawLine(26, 24, 30, 24, SSD1306_WHITE);

    display.drawLine(70, 3, 70, 7, SSD1306_WHITE);
    display.drawLine(68, 5, 72, 5, SSD1306_WHITE);
  }
}

void desenharMar() {
  for (int x = 0; x < SCREEN_WIDTH; x += 12) {
    int deslocamento = ((x / 12) % 2 == 0) ? 0 : 2;
    if (onda) deslocamento = 2 - deslocamento;

    display.drawLine(x, MAR_Y + deslocamento, x + 5, MAR_Y + 2 - deslocamento, SSD1306_WHITE);
    display.drawLine(x + 5, MAR_Y + 2 - deslocamento, x + 11, MAR_Y + deslocamento, SSD1306_WHITE);
  }

  display.drawFastHLine(0, 61, SCREEN_WIDTH, SSD1306_WHITE);
}

void desenharPenhasco() {
  display.fillTriangle(
    78, 61,
    127, 61,
    127, 43,
    SSD1306_WHITE
  );

  // Recortes deixam o rochedo mais irregular.
  display.fillTriangle(81, 61, 90, 54, 96, 61, SSD1306_BLACK);
  display.fillTriangle(106, 61, 113, 51, 119, 61, SSD1306_BLACK);
}

void desenharFarol() {
  // Torre levemente afunilada.
  display.fillTriangle(
    FAROL_X - 10, 52,
    FAROL_X + 10, 52,
    FAROL_X + 5, 24,
    SSD1306_WHITE
  );

  display.fillTriangle(
    FAROL_X - 10, 52,
    FAROL_X - 5, 24,
    FAROL_X + 5, 24,
    SSD1306_WHITE
  );

  // Faixas escuras.
  display.fillRect(FAROL_X - 6, 31, 12, 3, SSD1306_BLACK);
  display.fillRect(FAROL_X - 8, 41, 16, 3, SSD1306_BLACK);

  // Porta.
  display.fillRoundRect(FAROL_X - 3, 45, 6, 7, 2, SSD1306_BLACK);

  // Sala da lanterna.
  display.drawRect(FAROL_X - 7, 17, 14, 8, SSD1306_WHITE);
  display.fillRect(FAROL_X - 5, 19, 10, 4, SSD1306_BLACK);

  // Luz central.
  display.fillCircle(LUZ_X, LUZ_Y + 2, brilho ? 2 : 1, SSD1306_WHITE);

  // Telhado.
  display.fillTriangle(
    FAROL_X - 9, 17,
    FAROL_X + 9, 17,
    FAROL_X, 11,
    SSD1306_WHITE
  );
}

void desenharFeixe() {
  // O feixe varre a parte esquerda da tela.
  int abertura = 5;

  display.drawLine(LUZ_X - 2, LUZ_Y + 2, 0, feixeY - abertura, SSD1306_WHITE);
  display.drawLine(LUZ_X - 2, LUZ_Y + 2, 0, feixeY + abertura, SSD1306_WHITE);

  // Raios intermediarios deixam o cone mais visivel sem preencher toda a tela.
  display.drawLine(LUZ_X - 3, LUZ_Y + 2, 25, feixeY - 2, SSD1306_WHITE);
  display.drawLine(LUZ_X - 3, LUZ_Y + 2, 25, feixeY + 2, SSD1306_WHITE);
}

void desenharBarco(int x, int y, int dir) {
  // Casco.
  display.fillTriangle(x - 9, y, x + 9, y, x + 5, y + 5, SSD1306_WHITE);
  display.fillTriangle(x - 9, y, x - 5, y + 5, x + 5, y + 5, SSD1306_WHITE);

  // Mastro.
  display.drawFastVLine(x, y - 11, 11, SSD1306_WHITE);

  // Vela apontando no sentido do movimento.
  if (dir > 0) {
    display.drawTriangle(x + 1, y - 10, x + 1, y - 1, x + 8, y - 2, SSD1306_WHITE);
  } else {
    display.drawTriangle(x - 1, y - 10, x - 1, y - 1, x - 8, y - 2, SSD1306_WHITE);
  }
}

void atualizarAnimacao() {
  unsigned long agora = millis();

  if (agora - ultimoFeixe >= 85) {
    ultimoFeixe = agora;
    feixeY += direcaoFeixe * 2;

    if (feixeY >= 39 || feixeY <= 5) {
      direcaoFeixe *= -1;
    }
  }

  if (agora - ultimaOnda >= 240) {
    ultimaOnda = agora;
    onda = !onda;
  }

  if (agora - ultimoBrilho >= 430) {
    ultimoBrilho = agora;
    brilho = !brilho;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) return;
  ultimoFrame = agora;

  barcoX += direcaoBarco;

  if (direcaoBarco > 0 && barcoX > SCREEN_WIDTH + 18) {
    direcaoBarco = -1;
    barcoX = SCREEN_WIDTH + 18;
  }
  else if (direcaoBarco < 0 && barcoX < -18) {
    direcaoBarco = 1;
    barcoX = -18;
  }

  display.clearDisplay();

  desenharEstrelas();
  desenharFeixe();
  desenharMar();
  desenharBarco(barcoX, MAR_Y - 3, direcaoBarco);
  desenharPenhasco();
  desenharFarol();

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

  Serial.println(F("Farol iniciado"));
}

void loop() {
  atualizarAnimacao();
}

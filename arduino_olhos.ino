#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {
      // Interrompe a execucao se o display nao inicializar.
    }
  }

  // Inicializa os olhos: largura, altura e FPS maximo.
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);

  // Formato dos olhos.
  roboEyes.setWidth(36, 36);
  roboEyes.setHeight(36, 36);
  roboEyes.setBorderradius(8, 8);
  roboEyes.setSpacebetween(10);

  // Expressao inicial.
  roboEyes.setMood(DEFAULT);
  roboEyes.setCuriosity(ON);

  // Comportamentos automaticos.
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);
}

void loop() {
  // Mantem as animacoes fluidas. Evite delay() no loop.
  roboEyes.update();
}

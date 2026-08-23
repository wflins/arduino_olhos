#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int GROUND_Y = 56;
const int FRAME_INTERVAL = 70;

unsigned long lastFrame = 0;
int frameCounter = 0;

void drawGround() {
  display.drawFastHLine(0, GROUND_Y, SCREEN_WIDTH, SSD1306_WHITE);
  for (int x = 0; x < SCREEN_WIDTH; x += 8) {
    display.drawPixel(x, GROUND_Y + 2, SSD1306_WHITE);
    display.drawPixel(x + 4, GROUND_Y + 4, SSD1306_WHITE);
  }
}

void drawBrick(int x, int y, bool hitEffect = false) {
  display.drawRect(x, y, 16, 8, SSD1306_WHITE);
  display.drawFastHLine(x, y + 4, 16, SSD1306_WHITE);
  display.drawFastVLine(x + 8, y, 8, SSD1306_WHITE);

  if (hitEffect) {
    display.drawPixel(x - 2, y - 2, SSD1306_WHITE);
    display.drawPixel(x + 18, y - 1, SSD1306_WHITE);
    display.drawPixel(x + 7, y - 4, SSD1306_WHITE);
    display.drawPixel(x + 10, y - 5, SSD1306_WHITE);
  }
}

void drawTurtle(int x, int y, bool squashed = false, int legPhase = 0) {
  if (squashed) {
    display.drawRoundRect(x, y + 4, 14, 6, 2, SSD1306_WHITE);
  } else {
    display.drawRoundRect(x, y + 2, 14, 8, 2, SSD1306_WHITE);
  }

  display.fillCircle(x + 15, y + (squashed ? 7 : 6), 2, SSD1306_WHITE);

  if (!squashed) {
    if (legPhase == 0) {
      display.drawPixel(x + 2, y + 11, SSD1306_WHITE);
      display.drawPixel(x + 5, y + 10, SSD1306_WHITE);
      display.drawPixel(x + 9, y + 11, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 10, SSD1306_WHITE);
    } else {
      display.drawPixel(x + 2, y + 10, SSD1306_WHITE);
      display.drawPixel(x + 5, y + 11, SSD1306_WHITE);
      display.drawPixel(x + 9, y + 10, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 11, SSD1306_WHITE);
    }
  } else {
    display.drawFastHLine(x + 2, y + 10, 10, SSD1306_WHITE);
  }
}

void drawMario(int x, int y, int pose, bool airborne = false) {
  display.fillCircle(x + 5, y + 4, 3, SSD1306_WHITE);
  display.drawFastHLine(x + 1, y + 1, 8, SSD1306_WHITE);
  display.drawFastHLine(x + 2, y + 0, 5, SSD1306_WHITE);

  display.drawPixel(x + 8, y + 5, SSD1306_WHITE);
  display.drawFastHLine(x + 4, y + 7, 4, SSD1306_WHITE);

  display.drawFastVLine(x + 5, y + 8, 7, SSD1306_WHITE);
  display.drawFastVLine(x + 6, y + 8, 7, SSD1306_WHITE);
  display.drawPixel(x + 4, y + 10, SSD1306_WHITE);
  display.drawPixel(x + 7, y + 10, SSD1306_WHITE);

  if (pose == 0) {
    display.drawLine(x + 3, y + 9, x + 1, y + 12, SSD1306_WHITE);
    display.drawLine(x + 8, y + 9, x + 10, y + 7, SSD1306_WHITE);
    display.drawLine(x + 5, y + 15, x + 2, y + 18, SSD1306_WHITE);
    display.drawLine(x + 6, y + 15, x + 9, y + 18, SSD1306_WHITE);
  } else if (pose == 1) {
    display.drawLine(x + 3, y + 9, x + 1, y + 7, SSD1306_WHITE);
    display.drawLine(x + 8, y + 9, x + 10, y + 12, SSD1306_WHITE);
    display.drawLine(x + 5, y + 15, x + 3, y + 18, SSD1306_WHITE);
    display.drawLine(x + 6, y + 15, x + 8, y + 17, SSD1306_WHITE);
  } else if (pose == 2) {
    display.drawLine(x + 3, y + 9, x + 1, y + 8, SSD1306_WHITE);
    display.drawLine(x + 8, y + 9, x + 10, y + 8, SSD1306_WHITE);
    display.drawLine(x + 5, y + 15, x + 3, y + 12, SSD1306_WHITE);
    display.drawLine(x + 6, y + 15, x + 9, y + 12, SSD1306_WHITE);
  } else if (pose == 3) {
    display.drawLine(x + 3, y + 9, x + 1, y + 7, SSD1306_WHITE);
    display.drawLine(x + 8, y + 9, x + 8, y + 3, SSD1306_WHITE);
    display.drawLine(x + 5, y + 15, x + 4, y + 18, SSD1306_WHITE);
    display.drawLine(x + 6, y + 15, x + 7, y + 18, SSD1306_WHITE);
  }
}

int jumpArc(int t, int duration, int height) {
  int half = duration / 2;
  if (t < half) {
    return map(t, 0, half, 0, height);
  }
  return map(t, half, duration, height, 0);
}

void drawBricksSection(bool hit) {
  drawBrick(88, 18, hit);
  drawBrick(104, 18, hit);
}

void drawScene() {
  display.clearDisplay();
  drawGround();

  int marioX = 0;
  int marioY = GROUND_Y - 19;
  int marioPose = 0;

  int turtleX = 58;
  int turtleY = GROUND_Y - 12;
  bool turtleSquashed = false;
  int turtleLeg = (frameCounter / 2) % 2;

  bool hitBricks = false;

  if (frameCounter <= 24) {
    marioX = 4 + frameCounter * 2;
    marioPose = (frameCounter % 2 == 0) ? 0 : 1;
  } else if (frameCounter <= 44) {
    int t = frameCounter - 25;
    marioX = 52 + t * 2;
    int jump = jumpArc(t, 20, 22);
    marioY = (GROUND_Y - 19) - jump;
    marioPose = 2;

    if (t >= 10) {
      turtleSquashed = true;
    }
  } else if (frameCounter <= 64) {
    int t = frameCounter - 45;
    marioX = 80 + t;
    if (marioX > 95) marioX = 95;

    if (t < 8) {
      marioPose = (t % 2 == 0) ? 0 : 1;
    } else {
      int tj = t - 8;
      int jump = jumpArc(tj, 12, 18);
      marioY = (GROUND_Y - 19) - jump;
      marioPose = 3;
    }

    turtleSquashed = true;
  } else if (frameCounter <= 84) {
    int t = frameCounter - 65;
    marioX = 95;
    marioPose = 3;

    int jump = jumpArc((t % 12), 12, 18);
    marioY = (GROUND_Y - 19) - jump;

    if (t >= 4 && t <= 8) {
      hitBricks = true;
    }

    turtleSquashed = true;
  }

  drawBricksSection(hitBricks);

  if (frameCounter <= 24) {
    drawTurtle(turtleX, turtleY, false, turtleLeg);
  } else if (frameCounter <= 44) {
    drawTurtle(turtleX, turtleY, turtleSquashed, turtleLeg);
  } else {
    drawTurtle(turtleX, turtleY, true, turtleLeg);
  }

  drawMario(marioX, marioY, marioPose, marioPose == 2 || marioPose == 3);

  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false)) {
    Serial.println("SSD1306 nao encontrado");
    for (;;) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.display();

  Serial.println("Mario animation iniciado");
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame >= FRAME_INTERVAL) {
    lastFrame = now;

    drawScene();

    frameCounter++;
    if (frameCounter > 84) {
      frameCounter = 0;
    }
  }
}

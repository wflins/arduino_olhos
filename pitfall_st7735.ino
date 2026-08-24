#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const uint16_t FRAME_MS = 33; // ~30 FPS
const int GAME_Y = 0;
const int GAME_H = 128;
GFXcanvas16 fb(160, GAME_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long sceneStart = 0;

uint16_t C_DARKGREEN, C_LIGHTGREEN, C_YELLOW, C_BROWN, C_DARKBROWN;
uint16_t C_SKIN, C_SHIRT, C_PANTS, C_WATER, C_GOLD;

enum SceneType : uint8_t { VINE=0, LOG=1, CROCS=2, SCORPION=3, CAVE=4 };
SceneType scene = VINE;

int heroX = 34;
const int groundY = 91;
int obstacleX = 178;

void drawHud(){
  fb.setTextWrap(false);
  fb.setTextColor(ST77XX_WHITE);
  fb.setTextSize(2);
  fb.setCursor(50, 5);
  fb.print("1761");
  fb.setCursor(28, 24);
  fb.print("II 19:07");
}

void drawForestBase(){
  fb.fillScreen(C_DARKGREEN);
  fb.fillRect(0, 43, 160, 45, C_LIGHTGREEN);

  // Copas arredondadas e troncos, lembrando o visual do Atari.
  int scroll = (frameNo * 2) % 42;
  for(int x=-30; x<210; x+=42){
    int xx=x-scroll;
    fb.fillCircle(xx+12, 48, 15, C_LIGHTGREEN);
    fb.fillCircle(xx+27, 49, 13, C_LIGHTGREEN);
    fb.fillRect(xx+20, 49, 5, 39, C_BROWN);
  }

  // Faixa amarela de solo e base marrom.
  fb.fillRect(0, 88, 160, 10, C_YELLOW);
  fb.fillRect(0, 98, 160, 10, C_DARKBROWN);
}

void drawBottomBand(){
  fb.fillRect(0, 109, 160, 19, ST77XX_BLACK);
  fb.fillRect(5, 118, 150, 3, C_YELLOW);
  fb.setTextSize(1);
  fb.setTextColor(ST77XX_WHITE);
  fb.setCursor(44, 111);
  fb.print("ACTIVISION");
}

void drawHarry(int x, int baseY, int jump, bool swing=false){
  int y = baseY - jump;
  bool gait = ((frameNo/3)&1)==0;

  fb.fillCircle(x, y-14, 3, C_SKIN);
  fb.drawFastHLine(x-2, y-17, 5, ST77XX_BLACK);
  fb.fillRect(x-2, y-10, 5, 7, C_SHIRT);

  if(swing){
    fb.drawLine(x-1,y-8,x+6,y-17,C_SKIN);
    fb.drawLine(x+2,y-8,x+8,y-16,C_SKIN);
    fb.drawLine(x,y-3,x-5,y+4,C_PANTS);
    fb.drawLine(x+1,y-3,x+5,y+3,C_PANTS);
    return;
  }

  if(gait){
    fb.drawLine(x-1,y-8,x-6,y-4,C_SKIN);
    fb.drawLine(x+2,y-8,x+6,y-12,C_SKIN);
    fb.drawLine(x,y-3,x-5,y+4,C_PANTS);
    fb.drawLine(x+1,y-3,x+6,y+2,C_PANTS);
  } else {
    fb.drawLine(x-1,y-8,x-5,y-12,C_SKIN);
    fb.drawLine(x+2,y-8,x+7,y-4,C_SKIN);
    fb.drawLine(x,y-3,x-2,y+3,C_PANTS);
    fb.drawLine(x+1,y-3,x+7,y+4,C_PANTS);
  }
}

void drawPit(int x, int w){
  fb.fillRect(x, 88, w, 10, ST77XX_BLACK);
  fb.fillTriangle(x-5,88,x,88,x,98,C_DARKBROWN);
  fb.fillTriangle(x+w,88,x+w+5,88,x+w,98,C_DARKBROWN);
}

void drawVine(){
  int topX=92;
  int swing = (int)((frameNo % 120) - 60);
  int endX = topX + swing/3;
  int endY = 67 + abs(swing)/8;
  fb.drawLine(topX, 38, endX, endY, ST77XX_BLACK);

  drawPit(58, 54);

  if(frameNo%120 < 88){
    drawHarry(endX, endY+16, 0, true);
  } else {
    drawHarry(126, groundY, 0, false);
  }
}

void drawRollingLog(){
  int x = obstacleX;
  fb.fillRoundRect(x, 82, 27, 7, 3, C_BROWN);
  fb.drawCircle(x+5,85,3,C_DARKBROWN);
  fb.drawCircle(x+21,85,3,C_DARKBROWN);

  int dist=x-heroX;
  int jump=0;
  if(dist>-14 && dist<48){
    int t=48-dist;
    jump=(int)(sin((t*PI)/62.0f)*18.0f);
    if(jump<0) jump=0;
  }
  drawHarry(heroX, groundY, jump, false);
}

void drawCrocs(){
  drawPit(47, 70);
  uint16_t g=tft.color565(45,125,35);
  for(int i=0;i<3;i++){
    int x=57+i*22;
    bool open=((frameNo/9+i)&1)==0;
    fb.fillRect(x,90,15,4,g);
    fb.fillTriangle(x+15,90,x+21,92,x+15,94,g);
    if(open){
      fb.drawLine(x+10,89,x+16,86,g);
      fb.drawLine(x+10,95,x+16,98,g);
    }
    fb.drawPixel(x+4,89,ST77XX_WHITE);
  }

  int phase=frameNo%130;
  int jump=0;
  int hx=heroX;
  if(phase>35 && phase<100){
    hx=35+(phase-35)*2;
    int p=phase-35;
    jump=(int)(sin((p*PI)/65.0f)*22.0f);
  }
  drawHarry(hx, groundY, jump, false);
}

void drawScorpion(){
  int sx=obstacleX;
  uint16_t sc=tft.color565(235,235,225);
  fb.fillCircle(sx,87,4,sc);
  fb.fillCircle(sx+5,86,3,sc);
  fb.drawLine(sx+7,84,sx+10,80,sc);
  fb.drawLine(sx+10,80,sx+8,77,sc);
  fb.drawLine(sx-3,90,sx-8,93,sc);
  fb.drawLine(sx+2,90,sx+7,93,sc);

  int dist=sx-heroX;
  int jump=0;
  if(dist>-10 && dist<44){
    int t=44-dist;
    jump=(int)(sin((t*PI)/54.0f)*16.0f);
    if(jump<0) jump=0;
  }
  drawHarry(heroX, groundY, jump, false);
}

void drawCave(){
  fb.fillScreen(ST77XX_BLACK);
  fb.fillRect(0, 20, 160, 18, C_DARKBROWN);
  fb.fillRect(0, 91, 160, 17, C_BROWN);

  for(int x=0;x<160;x+=24){
    fb.fillTriangle(x,38,x+8,38,x+4,50,C_DARKBROWN);
  }

  int scroll=(frameNo*2)%55;
  for(int x=-30;x<210;x+=55){
    int xx=x-scroll;
    fb.fillRect(xx,76,10,15,C_DARKBROWN);
    fb.fillRect(xx+2,72,6,5,C_GOLD);
  }

  drawHarry(heroX, 91, 0, false);
}

void updateScene(){
  unsigned long now=millis();
  if(now-sceneStart >= 8500UL){
    scene=(SceneType)(((uint8_t)scene+1)%5);
    sceneStart=now;
    obstacleX=178;
  }
}

void updateObjects(){
  if(scene==LOG || scene==SCORPION){
    obstacleX-=3;
    if(obstacleX<-35) obstacleX=178;
  }
}

void renderFrame(){
  if(scene==CAVE){
    drawCave();
  } else {
    drawForestBase();
    drawHud();

    switch(scene){
      case VINE: drawVine(); break;
      case LOG: drawRollingLog(); break;
      case CROCS: drawCrocs(); break;
      case SCORPION: drawScorpion(); break;
      default: break;
    }
  }

  drawBottomBand();
  tft.drawRGBBitmap(0, GAME_Y, fb.getBuffer(), 160, GAME_H);
}

void setup(){
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  C_DARKGREEN=tft.color565(0,95,15);
  C_LIGHTGREEN=tft.color565(105,230,15);
  C_YELLOW=tft.color565(255,225,65);
  C_BROWN=tft.color565(115,55,12);
  C_DARKBROWN=tft.color565(145,110,0);
  C_SKIN=tft.color565(245,180,120);
  C_SHIRT=tft.color565(245,95,180);
  C_PANTS=tft.color565(20,90,50);
  C_WATER=tft.color565(35,105,180);
  C_GOLD=tft.color565(255,195,20);

  lastFrame=sceneStart=millis();
}

void loop(){
  unsigned long now=millis();
  updateScene();
  if(now-lastFrame>=FRAME_MS){
    lastFrame=now;
    frameNo++;
    updateObjects();
    renderFrame();
  }
  yield();
}

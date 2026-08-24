#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const uint16_t FRAME_MS = 33; // ~30 FPS
const int SCENE_Y = 18;
const int SCENE_H = 92;
GFXcanvas16 framebuf(160, SCENE_H);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long fpsStart = 0;
uint16_t fpsFrames = 0, fpsValue = 0;

// 4 paisagens que alternam automaticamente.
enum SceneType : uint8_t { JUNGLE=0, LOGS=1, SWAMP=2, CAVE=3 };
SceneType scene = JUNGLE;
unsigned long sceneStart = 0;

int heroX = 43;
const int heroGroundY = 68;
int obstacleX = 185;
uint8_t obstacleKind = 0;

uint16_t C_SKY, C_LEAF1, C_LEAF2, C_TRUNK, C_DIRT, C_WATER, C_MUD, C_CAVE;

void drawHeader(){
  tft.fillRect(0,0,160,SCENE_Y,ST77XX_BLACK);
  tft.setTextWrap(false); tft.setTextSize(1);
  tft.setTextColor(tft.color565(90,230,100));
  tft.setCursor(4,3); tft.print("PITFALL ST7735");
  tft.setTextColor(ST77XX_WHITE); tft.setCursor(105,3); tft.print("FPS:");
  tft.fillRect(134,3,24,8,ST77XX_BLACK); tft.setCursor(134,3); tft.setTextColor(ST77XX_GREEN); tft.print(fpsValue);
}

void drawTree(int x,int base,int s){
  framebuf.fillRect(x-2*s, base-18*s, 4*s, 18*s, C_TRUNK);
  framebuf.fillCircle(x, base-20*s, 9*s, C_LEAF1);
  framebuf.fillCircle(x-7*s, base-17*s, 7*s, C_LEAF2);
  framebuf.fillCircle(x+7*s, base-17*s, 7*s, C_LEAF2);
}

void drawVines(int phase){
  uint16_t vine=tft.color565(25,120,35);
  for(int x=-20;x<190;x+=38){
    int xx=x-(phase%38);
    framebuf.drawLine(xx,0,xx+3,14,vine);
    framebuf.drawLine(xx+3,14,xx-2,24,vine);
  }
}

void drawJungleBase(){
  framebuf.fillScreen(C_SKY);
  framebuf.fillRect(0,52,160,40,C_DIRT);
  int scroll=(frameNo*2)%52;
  for(int x=-25;x<210;x+=52) drawTree(x-scroll+15,55,1);
  drawVines(frameNo*2);
  // grama
  for(int x=0;x<160;x+=7) framebuf.drawFastVLine(x,51,5,C_LEAF1);
}

void drawLogScene(){
  framebuf.fillScreen(tft.color565(105,185,220));
  framebuf.fillRect(0,50,160,42,C_DIRT);
  int scroll=(frameNo*2)%60;
  for(int x=-30;x<200;x+=60){
    int xx=x-scroll;
    framebuf.fillRect(xx,36,7,16,C_TRUNK);
    framebuf.fillCircle(xx+3,34,12,C_LEAF1);
  }
  // troncos caidos no plano de fundo
  for(int x=-40;x<200;x+=70){ int xx=x-((frameNo*3)%70); framebuf.fillRoundRect(xx,57,28,6,3,tft.color565(110,60,20)); }
}

void drawSwampScene(){
  framebuf.fillScreen(tft.color565(70,150,180));
  framebuf.fillRect(0,42,160,50,C_WATER);
  int wave=(frameNo*2)%12;
  for(int y=48;y<90;y+=12){
    for(int x=-20;x<180;x+=24) framebuf.drawFastHLine(x-wave,y,12,tft.color565(120,210,210));
  }
  // ilhotas
  int s=(frameNo*2)%80;
  for(int x=-60;x<220;x+=80){ int xx=x-s; framebuf.fillCircle(xx,66,10,C_MUD); framebuf.fillRect(xx-12,66,24,7,C_MUD); }
  // árvores distantes
  for(int x=12;x<160;x+=45){ framebuf.fillRect(x,25,4,20,C_TRUNK); framebuf.fillCircle(x+2,22,10,C_LEAF2); }
}

void drawCaveScene(){
  framebuf.fillScreen(C_CAVE);
  // teto e chão irregulares
  framebuf.fillRect(0,0,160,12,tft.color565(55,45,40));
  framebuf.fillRect(0,66,160,26,tft.color565(70,50,35));
  int scroll=(frameNo*2)%36;
  for(int x=-20;x<190;x+=36){ int xx=x-scroll; framebuf.fillTriangle(xx,12,xx+8,12,xx+4,24,tft.color565(95,80,70)); }
  for(int x=-15;x<190;x+=42){ int xx=x-scroll; framebuf.fillTriangle(xx,66,xx+10,66,xx+5,55,tft.color565(100,75,55)); }
  // tochas
  for(int x=-30;x<210;x+=70){ int xx=x-((frameNo*2)%70); framebuf.fillRect(xx,38,2,12,C_TRUNK); framebuf.fillCircle(xx+1,36,3,tft.color565(255,140,30)); }
}

void drawBackground(){
  switch(scene){
    case JUNGLE: drawJungleBase(); break;
    case LOGS: drawLogScene(); break;
    case SWAMP: drawSwampScene(); break;
    case CAVE: drawCaveScene(); break;
  }
}

void drawHero(int x,int baseY,int jump){
  int y=baseY-jump;
  uint16_t skin=tft.color565(235,175,105);
  uint16_t shirt=tft.color565(235,220,75);
  uint16_t pants=tft.color565(60,85,35);
  uint16_t hair=tft.color565(45,25,15);

  // cabeça / cabelo
  framebuf.fillCircle(x,y-19,4,skin);
  framebuf.drawFastHLine(x-3,y-22,6,hair);
  // tronco e braços
  framebuf.fillRect(x-3,y-14,7,10,shirt);
  bool gait=((frameNo/3)&1)==0;
  if(gait){
    framebuf.drawLine(x-2,y-12,x-8,y-6,skin); framebuf.drawLine(x+3,y-11,x+8,y-15,skin);
    framebuf.drawLine(x-1,y-4,x-7,y+5,pants); framebuf.drawLine(x+2,y-4,x+8,y+2,pants);
  }else{
    framebuf.drawLine(x-2,y-11,x-7,y-15,skin); framebuf.drawLine(x+3,y-12,x+9,y-6,skin);
    framebuf.drawLine(x-1,y-4,x-3,y+4,pants); framebuf.drawLine(x+2,y-4,x+9,y+5,pants);
  }
}

int computeJump(){
  int dist=obstacleX-heroX;
  if(dist>-14 && dist<55){
    int t=55-dist; // 0..69
    int phase=(t*180)/69;
    int s=(int)(sin(phase*PI/180.0f)*20.0f);
    return s<0?0:s;
  }
  return 0;
}

void drawObstacle(){
  if(scene==JUNGLE){
    if(obstacleKind&1){ // cobra
      framebuf.fillCircle(obstacleX,70,3,tft.color565(30,120,40));
      framebuf.drawLine(obstacleX+3,70,obstacleX+12,66,tft.color565(30,120,40));
      framebuf.drawPixel(obstacleX-1,69,ST77XX_RED);
    } else { // tronco
      framebuf.fillRoundRect(obstacleX,64,28,8,4,tft.color565(95,48,18));
      framebuf.drawFastHLine(obstacleX+4,66,20,tft.color565(175,105,45));
    }
  } else if(scene==LOGS){
    framebuf.fillRoundRect(obstacleX,62,32,9,4,tft.color565(100,50,18));
    framebuf.fillCircle(obstacleX+5,66,4,tft.color565(150,85,35));
    framebuf.fillCircle(obstacleX+26,66,4,tft.color565(150,85,35));
  } else if(scene==SWAMP){
    // crocodilo
    uint16_t g=tft.color565(45,120,50);
    framebuf.fillRect(obstacleX,64,30,5,g);
    framebuf.fillTriangle(obstacleX+30,64,obstacleX+38,67,obstacleX+30,69,g);
    framebuf.drawPixel(obstacleX+4,63,ST77XX_WHITE);
  } else {
    // poço / pedra
    framebuf.fillCircle(obstacleX+8,65,9,tft.color565(95,95,90));
    framebuf.fillCircle(obstacleX+17,68,6,tft.color565(75,75,70));
  }
}

void updateScene(){
  unsigned long now=millis();
  if(now-sceneStart>11000UL){ scene=(SceneType)(((uint8_t)scene+1)%4); sceneStart=now; obstacleX=185; obstacleKind++; }
}

void updateObstacle(){
  obstacleX-=3;
  if(obstacleX<-45){ obstacleX=170+random(20,70); obstacleKind++; }
}

void renderFrame(){
  drawBackground();
  drawObstacle();
  int jump=computeJump();
  drawHero(heroX,heroGroundY,jump);
  // foreground foliage/depth
  if(scene==JUNGLE||scene==LOGS){
    for(int x=-25;x<190;x+=50){ int xx=x-((frameNo*4)%50); framebuf.fillCircle(xx,86,8,C_LEAF2); }
  }
  tft.drawRGBBitmap(0,SCENE_Y,framebuf.getBuffer(),160,SCENE_H);
}

void setup(){
  Serial.begin(115200); randomSeed(micros());
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST77XX_BLACK);
  C_SKY=tft.color565(80,170,220); C_LEAF1=tft.color565(28,135,45); C_LEAF2=tft.color565(20,95,35);
  C_TRUNK=tft.color565(105,62,28); C_DIRT=tft.color565(150,92,38); C_WATER=tft.color565(35,115,165);
  C_MUD=tft.color565(110,80,45); C_CAVE=tft.color565(28,24,26);
  lastFrame=fpsStart=sceneStart=millis(); drawHeader();
}

void loop(){
  unsigned long now=millis(); updateScene();
  if(now-lastFrame>=FRAME_MS){ lastFrame=now; frameNo++; updateObstacle(); renderFrame(); fpsFrames++; }
  if(now-fpsStart>=1000UL){ fpsValue=fpsFrames; fpsFrames=0; fpsStart=now; drawHeader(); Serial.print("FPS: "); Serial.println(fpsValue); }
  yield();
}

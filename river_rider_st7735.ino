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

// Cenas: rio aberto, ilhas, ponte/base e cânion.
enum RiverScene : uint8_t { OPEN_RIVER=0, ISLANDS=1, BRIDGE=2, CANYON=3 };
RiverScene scene = OPEN_RIVER;
unsigned long sceneStart = 0;

int planeX = 80;
int targetX = 80;
int enemyY = -20;
int enemyLane = 0;
uint8_t enemyKind = 0;

uint16_t C_WATER, C_BANK, C_SAND, C_DARKBANK, C_SKY;

void drawHeader(){
  tft.fillRect(0,0,160,SCENE_Y,ST77XX_BLACK);
  tft.setTextWrap(false); tft.setTextSize(1);
  tft.setTextColor(tft.color565(80,180,255)); tft.setCursor(4,3); tft.print("RIVER RIDER ST7735");
  tft.setTextColor(ST77XX_WHITE); tft.setCursor(105,3); tft.print("FPS:");
  tft.fillRect(134,3,24,8,ST77XX_BLACK); tft.setCursor(134,3); tft.setTextColor(ST77XX_GREEN); tft.print(fpsValue);
}

inline int riverCenterAt(int y){
  // Curva suave usando lookup inteira/triangular para evitar float pesado.
  int p=(frameNo*2 + y*3)%120;
  int tri = p<60 ? p : 120-p;
  return 80 + (tri-30)/2;
}

inline int riverHalfWidthAt(int y){
  int w=31 + y/5;
  if(scene==CANYON) w-=8;
  if(scene==OPEN_RIVER) w+=5;
  return w;
}

void drawRiverBase(){
  framebuf.fillScreen(C_BANK);
  for(int y=0;y<SCENE_H;y++){
    int c=riverCenterAt(y);
    int hw=riverHalfWidthAt(y);
    int left=c-hw, right=c+hw;
    if(left<0) left=0; if(right>159) right=159;
    // areia / margem clara
    framebuf.drawFastHLine(left-3<0?0:left-3,y,min(3,left),C_SAND);
    framebuf.drawFastHLine(left,y,right-left+1,C_WATER);
    if(right<157) framebuf.drawFastHLine(right+1,y,3,C_SAND);
  }
}

void drawOpenRiver(){
  drawRiverBase();
  // bancos de areia e árvores laterais rolando para baixo
  int scroll=(frameNo*3)%48;
  for(int y=-20;y<110;y+=48){
    int yy=y+scroll;
    if(yy<0||yy>=SCENE_H) continue;
    int c=riverCenterAt(yy), hw=riverHalfWidthAt(yy);
    int lx=c-hw-8, rx=c+hw+8;
    framebuf.fillCircle(lx,yy,5,tft.color565(30,115,40));
    framebuf.fillCircle(rx,yy+5,5,tft.color565(30,115,40));
  }
}

void drawIslands(){
  drawRiverBase();
  int scroll=(frameNo*3)%110;
  for(int base=-40;base<180;base+=110){
    int y=base+scroll;
    if(y<0||y>=SCENE_H) continue;
    int c=riverCenterAt(y);
    int offset=((base/110)&1)?-13:14;
    int x=c+offset;
    framebuf.fillEllipse(x,y,12,6,C_SAND);
    framebuf.fillEllipse(x,y-1,8,4,tft.color565(60,145,55));
    framebuf.fillRect(x-1,y-7,2,6,tft.color565(95,60,25));
    framebuf.fillCircle(x,y-9,4,tft.color565(30,125,45));
  }
}

void drawBridge(){
  drawRiverBase();
  int bridgeY=(int)((frameNo*3)%140)-25;
  if(bridgeY>=-8 && bridgeY<SCENE_H+8){
    uint16_t metal=tft.color565(125,125,130);
    uint16_t dark=tft.color565(65,65,70);
    framebuf.fillRect(0,bridgeY,160,7,metal);
    for(int x=0;x<160;x+=16) framebuf.fillRect(x,bridgeY+1,7,5,dark);
    // abertura sobre o rio
    int c=riverCenterAt(constrain(bridgeY,0,SCENE_H-1));
    framebuf.fillRect(c-18,bridgeY-1,36,9,C_WATER);
    framebuf.drawFastHLine(c-18,bridgeY-1,36,ST77XX_WHITE);
  }
}

void drawCanyon(){
  drawRiverBase();
  // paredões rochosos nas margens
  int scroll=(frameNo*3)%36;
  for(int y=-15;y<110;y+=36){
    int yy=y+scroll; if(yy<0||yy>=SCENE_H) continue;
    int c=riverCenterAt(yy), hw=riverHalfWidthAt(yy);
    uint16_t rock=tft.color565(120,75,45);
    framebuf.fillTriangle(c-hw-2,yy-8,c-hw-15,yy+5,c-hw-2,yy+10,rock);
    framebuf.fillTriangle(c+hw+2,yy-8,c+hw+15,yy+5,c+hw+2,yy+10,rock);
  }
}

void drawBackground(){
  switch(scene){
    case OPEN_RIVER: drawOpenRiver(); break;
    case ISLANDS: drawIslands(); break;
    case BRIDGE: drawBridge(); break;
    case CANYON: drawCanyon(); break;
  }
}

void drawPlane(int x,int y){
  uint16_t silver=tft.color565(215,220,225);
  uint16_t dark=tft.color565(70,80,95);
  uint16_t glass=tft.color565(70,160,225);
  framebuf.fillTriangle(x,y-10,x-4,y+8,x+4,y+8,silver);
  framebuf.fillRect(x-13,y-1,26,4,silver);
  framebuf.fillTriangle(x-13,y-1,x-5,y+5,x-5,y-1,dark);
  framebuf.fillTriangle(x+13,y-1,x+5,y+5,x+5,y-1,dark);
  framebuf.fillRect(x-7,y+6,14,3,silver);
  framebuf.fillRect(x-1,y-6,3,5,glass);
  framebuf.drawPixel(x,y+8,ST77XX_RED);
}

void drawBoat(int x,int y,uint16_t color){
  framebuf.fillTriangle(x,y+6,x-6,y-5,x+6,y-5,color);
  framebuf.fillRect(x-3,y-8,6,4,ST77XX_WHITE);
  framebuf.drawPixel(x+1,y-7,ST77XX_BLACK);
}

void drawHelicopter(int x,int y,uint16_t color){
  framebuf.fillCircle(x,y,5,color);
  framebuf.fillRect(x-11,y-1,11,3,color);
  framebuf.drawFastHLine(x-9,y-7,18,ST77XX_WHITE);
  framebuf.drawFastVLine(x,y-6,6,ST77XX_WHITE);
  framebuf.fillRect(x+4,y+2,6,2,color);
}

void spawnEnemy(){
  enemyY=-14;
  enemyLane=random(-1,2);
  enemyKind=(enemyKind+1)%2;
}

void updateEnemy(){
  enemyY+=2;
  if(enemyY>SCENE_H+16) spawnEnemy();
}

int enemyXForY(int y){
  int c=riverCenterAt(constrain(y,0,SCENE_H-1));
  int hw=riverHalfWidthAt(constrain(y,0,SCENE_H-1));
  return c + enemyLane*(hw/2);
}

void updatePlaneAI(){
  // olha o inimigo quando ele se aproxima e escolhe lado oposto.
  if(enemyY>30 && enemyY<78){
    int ex=enemyXForY(enemyY);
    int c=riverCenterAt(72), hw=riverHalfWidthAt(72);
    targetX = ex<planeX ? c+hw/3 : c-hw/3;
  }else{
    targetX=riverCenterAt(72);
  }
  int c=riverCenterAt(72), hw=riverHalfWidthAt(72)-8;
  if(targetX<c-hw) targetX=c-hw; if(targetX>c+hw) targetX=c+hw;
  if(planeX<targetX) planeX+=2; else if(planeX>targetX) planeX-=2;
}

void drawEnemy(){
  if(enemyY<0||enemyY>=SCENE_H) return;
  int ex=enemyXForY(enemyY);
  if(enemyKind==0) drawBoat(ex,enemyY,tft.color565(230,55,45));
  else drawHelicopter(ex,enemyY,tft.color565(235,185,45));
}

void drawFuelDepot(){
  if(scene!=OPEN_RIVER && scene!=BRIDGE) return;
  int y=(int)((frameNo*2+55)%150)-30;
  if(y<0||y>=SCENE_H) return;
  int c=riverCenterAt(y), hw=riverHalfWidthAt(y);
  int x=c-hw/2;
  uint16_t fuel=tft.color565(235,210,60);
  framebuf.fillRect(x-5,y-5,10,10,fuel);
  framebuf.drawRect(x-5,y-5,10,10,ST77XX_WHITE);
  framebuf.drawFastVLine(x,y-4,8,ST77XX_BLACK);
}

void updateScene(){
  unsigned long now=millis();
  if(now-sceneStart>10000UL){
    scene=(RiverScene)(((uint8_t)scene+1)%4);
    sceneStart=now;
  }
}

void renderFrame(){
  drawBackground();
  drawFuelDepot();
  drawEnemy();
  drawPlane(planeX,72);
  // espuma/velocidade perto do avião
  int c=riverCenterAt(82);
  for(int i=-2;i<=2;i+=2) framebuf.drawFastVLine(c+i*8,83+((frameNo+i)&1),5,tft.color565(145,210,245));
  tft.drawRGBBitmap(0,SCENE_Y,framebuf.getBuffer(),160,SCENE_H);
}

void setup(){
  Serial.begin(115200); randomSeed(micros());
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST77XX_BLACK);
  C_WATER=tft.color565(30,95,205); C_BANK=tft.color565(55,145,55); C_SAND=tft.color565(195,180,95);
  C_DARKBANK=tft.color565(35,100,40); C_SKY=tft.color565(90,170,225);
  lastFrame=fpsStart=sceneStart=millis(); spawnEnemy(); drawHeader();
}

void loop(){
  unsigned long now=millis(); updateScene();
  if(now-lastFrame>=FRAME_MS){ lastFrame=now; frameNo++; updateEnemy(); updatePlaneAI(); renderFrame(); fpsFrames++; }
  if(now-fpsStart>=1000UL){ fpsValue=fpsFrames; fpsFrames=0; fpsStart=now; drawHeader(); Serial.print("FPS: "); Serial.println(fpsValue); }
  yield();
}

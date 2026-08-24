#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS D8
#define TFT_RST D1
#define TFT_DC D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const uint16_t FRAME_MS = 33;
GFXcanvas16 fb(160,128);
uint32_t frameNo=0;
unsigned long lastFrame=0, sceneStart=0;

// Paleta aproximada do Pitfall! original (Atari 2600)
uint16_t C_SKY, C_JUNGLE, C_TRUNK, C_GROUND, C_SOIL;
uint16_t C_SKIN, C_HAIR, C_SHIRT, C_PANTS;
uint16_t C_LOG, C_CROC, C_FIRE1, C_FIRE2, C_SNAKE, C_SCORP;

enum SceneType:uint8_t { VINE_PIT, ROLLING_LOGS, CROCODILES, RATTLESNAKE, CAMPFIRE, UNDERGROUND };
SceneType scene=VINE_PIT;
int objX=180;
const int groundY=92;

void drawHud(){
  fb.setTextWrap(false); fb.setTextColor(ST77XX_WHITE); fb.setTextSize(2);
  fb.setCursor(48,5); fb.print("1761");
  fb.setCursor(24,24); fb.print("II 19:07");
}

void drawForest(){
  fb.fillScreen(C_SKY);
  // copa escura superior
  fb.fillRect(0,0,160,44,C_SKY);
  // massa verde clara inferior
  fb.fillRect(0,44,160,44,C_JUNGLE);

  // recorte irregular simples nas copas, como no Atari
  for(int x=0;x<160;x+=16){
    int h=((x/16)&1)?4:8;
    fb.fillRect(x,40+h,16,8-h,C_JUNGLE);
  }

  // troncos finos verticais e praticamente fixos
  const int trunks[]={15,48,80,112,145};
  for(int i=0;i<5;i++) fb.fillRect(trunks[i],47,4,41,C_TRUNK);

  // faixa amarela e subsolo ocre
  fb.fillRect(0,88,160,11,C_GROUND);
  fb.fillRect(0,99,160,10,C_SOIL);
  drawHud();
}

void drawBottom(){
  fb.fillRect(0,109,160,19,ST77XX_BLACK);
  fb.fillRect(5,118,150,3,C_SOIL);
  fb.setTextSize(1); fb.setTextColor(ST77XX_WHITE);
  fb.setCursor(47,111); fb.print("ACTIVISION");
}

void drawHarry(int x,int baseY,int jump,bool swing=false){
  int y=baseY-jump;
  bool p=((frameNo/3)&1)==0;
  // sprite pequeno, propositalmente simples/pixelado
  fb.fillRect(x-2,y-16,5,4,C_SKIN);
  fb.drawFastHLine(x-2,y-17,5,C_HAIR);
  fb.fillRect(x-2,y-12,5,7,C_SHIRT);
  if(swing){
    fb.drawLine(x,y-11,x+5,y-17,C_SKIN);
    fb.drawLine(x+2,y-10,x+6,y-16,C_SKIN);
    fb.drawLine(x,y-5,x-4,y+1,C_PANTS);
    fb.drawLine(x+1,y-5,x+5,y,C_PANTS);
  }else if(p){
    fb.drawLine(x-1,y-10,x-5,y-6,C_SKIN);
    fb.drawLine(x+2,y-10,x+6,y-13,C_SKIN);
    fb.drawLine(x,y-5,x-5,y+2,C_PANTS);
    fb.drawLine(x+1,y-5,x+6,y,C_PANTS);
  }else{
    fb.drawLine(x-1,y-10,x-5,y-13,C_SKIN);
    fb.drawLine(x+2,y-10,x+6,y-6,C_SKIN);
    fb.drawLine(x,y-5,x-2,y+1,C_PANTS);
    fb.drawLine(x+1,y-5,x+6,y+2,C_PANTS);
  }
}

void drawPit(int x,int w){
  fb.fillRect(x,88,w,11,ST77XX_BLACK);
  fb.fillTriangle(x-4,88,x,88,x,99,C_SOIL);
  fb.fillTriangle(x+w,88,x+w+4,88,x+w,99,C_SOIL);
}

void sceneVine(){
  drawPit(58,52);
  const int topX=92;
  int phase=frameNo%120;
  int offs=(phase<60?phase:119-phase)-30;
  int endX=topX+offs/2;
  int endY=67+abs(offs)/7;
  fb.drawLine(topX,42,endX,endY,ST77XX_BLACK);
  drawHarry(endX,endY+16,0,true);
}

void drawLog(int x){
  fb.fillRect(x,84,25,5,C_LOG);
  fb.drawFastHLine(x+3,83,19,C_LOG);
  fb.drawPixel(x+2,88,ST77XX_BLACK);
  fb.drawPixel(x+22,88,ST77XX_BLACK);
}

int jumpFor(int dist,int maxh=17){
  if(dist<-10||dist>48) return 0;
  int t=48-dist; // 0..58
  int a=t<=29?t:58-t;
  if(a<0)a=0;
  return (a*maxh)/29;
}

void sceneLogs(){
  drawLog(objX); drawLog(objX+46); drawLog(objX+92);
  drawHarry(34,groundY,jumpFor(objX-34),false);
}

void drawCroc(int x,bool open){
  fb.fillRect(x,90,15,4,C_CROC);
  fb.fillTriangle(x+15,90,x+22,92,x+15,94,C_CROC);
  fb.drawPixel(x+4,89,ST77XX_WHITE);
  if(open){
    fb.drawLine(x+10,89,x+17,86,C_CROC);
    fb.drawLine(x+10,95,x+17,98,C_CROC);
  }
}

void sceneCrocs(){
  drawPit(45,74);
  for(int i=0;i<3;i++) drawCroc(54+i*23,((frameNo/8+i)&1)==0);
  int ph=frameNo%120; int hx=31,j=0;
  if(ph>28&&ph<90){ int p=ph-28; hx=31+p*2; int tri=p<31?p:61-p; if(tri<0)tri=0; j=(tri*21)/31; }
  drawHarry(hx,groundY,j,false);
}

void drawRattler(int x){
  // cascavel escura em S, mais fiel que uma cobra genérica
  fb.fillCircle(x,88,3,C_SNAKE);
  fb.fillCircle(x+5,86,3,C_SNAKE);
  fb.fillCircle(x+9,89,3,C_SNAKE);
  fb.drawLine(x+10,87,x+14,82,C_SNAKE);
  fb.drawLine(x+14,82,x+11,78,C_SNAKE);
  fb.drawPixel(x+12,78,ST77XX_WHITE);
}

void sceneSnake(){
  drawRattler(objX);
  drawHarry(34,groundY,jumpFor(objX-34,15),false);
}

void drawFire(int x){
  fb.fillRect(x-4,87,10,3,C_LOG);
  bool p=((frameNo/3)&1)==0;
  fb.fillTriangle(x,87,x-5,79,x+5,79,C_FIRE1);
  fb.fillTriangle(x,86,x-3,81,x+3,81,p?C_FIRE2:C_GROUND);
}

void sceneFire(){
  drawFire(objX);
  drawHarry(34,groundY,jumpFor(objX-34,18),false);
}

void drawScorpion(int x){
  // escorpião claro do subsolo, com cauda levantada
  fb.fillCircle(x,92,3,C_SCORP);
  fb.fillCircle(x+5,91,3,C_SCORP);
  fb.drawLine(x+7,89,x+10,85,C_SCORP);
  fb.drawLine(x+10,85,x+9,81,C_SCORP);
  fb.drawLine(x+9,81,x+6,79,C_SCORP);
  fb.drawLine(x-2,94,x-7,97,C_SCORP);
  fb.drawLine(x+3,94,x+8,97,C_SCORP);
  fb.drawLine(x+1,94,x-2,98,C_SCORP);
}

void sceneUnderground(){
  fb.fillScreen(ST77XX_BLACK);
  fb.fillRect(0,18,160,18,C_SOIL);
  fb.fillRect(0,96,160,13,C_SOIL);
  for(int x=0;x<160;x+=24) fb.fillTriangle(x,36,x+8,36,x+4,46,C_SOIL);
  drawHud();
  drawScorpion(objX);
  drawHarry(34,96,jumpFor(objX-34,14),false);
}

void updateScene(){
  unsigned long now=millis();
  if(now-sceneStart>=8000UL){ scene=(SceneType)(((uint8_t)scene+1)%6); sceneStart=now; objX=180; }
}

void updateObjects(){
  if(scene==ROLLING_LOGS||scene==RATTLESNAKE||scene==CAMPFIRE||scene==UNDERGROUND){
    objX-=3; if(objX<-100)objX=180;
  }
}

void renderFrame(){
  if(scene!=UNDERGROUND) drawForest();
  switch(scene){
    case VINE_PIT: sceneVine(); break;
    case ROLLING_LOGS: sceneLogs(); break;
    case CROCODILES: sceneCrocs(); break;
    case RATTLESNAKE: sceneSnake(); break;
    case CAMPFIRE: sceneFire(); break;
    case UNDERGROUND: sceneUnderground(); break;
  }
  drawBottom();
  tft.drawRGBBitmap(0,0,fb.getBuffer(),160,128);
}

void setup(){
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST77XX_BLACK);

  C_SKY=tft.color565(0,92,8);          // verde escuro do topo
  C_JUNGLE=tft.color565(104,222,0);    // verde lima do bosque
  C_TRUNK=tft.color565(92,43,7);       // marrom tronco
  C_GROUND=tft.color565(245,216,48);   // amarelo da superfície
  C_SOIL=tft.color565(142,106,0);      // ocre inferior
  C_SKIN=tft.color565(244,164,120);
  C_HAIR=tft.color565(45,20,8);
  C_SHIRT=tft.color565(85,205,95);     // camisa verde do Harry
  C_PANTS=tft.color565(18,82,48);
  C_LOG=tft.color565(45,25,10);
  C_CROC=tft.color565(20,90,25);
  C_FIRE1=tft.color565(245,70,24);
  C_FIRE2=tft.color565(255,185,35);
  C_SNAKE=tft.color565(20,20,12);
  C_SCORP=tft.color565(235,235,220);

  lastFrame=sceneStart=millis();
}

void loop(){
  unsigned long now=millis(); updateScene();
  if(now-lastFrame>=FRAME_MS){ lastFrame=now; frameNo++; updateObjects(); renderFrame(); }
  yield();
}

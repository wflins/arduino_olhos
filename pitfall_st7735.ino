#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  D8
#define TFT_RST D1
#define TFT_DC  D2

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Pitfall ST7735 - visual inspirado de perto no Pitfall! do Atari 2600.
// Prioridade: proporcoes, paleta, HUD, sprites pixelados e fluidez.

const uint16_t FRAME_MS = 33; // ~30 FPS
GFXcanvas16 fb(160, 128);

uint32_t frameNo = 0;
unsigned long lastFrame = 0;
unsigned long sceneStart = 0;

uint16_t C_SKY, C_CANOPY, C_FIELD, C_TRUNK, C_GROUND, C_GROUND2;
uint16_t C_SKIN, C_SHIRT, C_PANTS, C_SCORPION, C_CROC, C_GOLD;

enum SceneType : uint8_t {
  SCENE_VINE = 0,
  SCENE_LOG = 1,
  SCENE_CROCS = 2,
  SCENE_SCORPION = 3,
  SCENE_UNDERGROUND = 4
};

SceneType scene = SCENE_VINE;
int movingX = 182;

const int PLAY_TOP = 6;
const int CANOPY_BOTTOM = 46;
const int GROUND_Y = 88;
const int LOWER_BLACK_Y = 108;

void putPixelBlock(int x,int y,int w,int h,uint16_t c){
  fb.fillRect(x,y,w,h,c);
}

void drawHud(){
  fb.setTextWrap(false);
  fb.setTextColor(ST77XX_WHITE);
  fb.setTextSize(2);
  fb.setCursor(50,7);
  fb.print("1761");
  fb.setCursor(25,24);
  fb.print("II 19:07");
}

void drawCanopy(){
  // fundo verde escuro superior
  fb.fillRect(0,0,160,CANOPY_BOTTOM,C_SKY);

  // borda irregular inferior da copa, usando blocos grandes estilo Atari
  const int y = 42;
  for(int x=0;x<160;x+=8){
    int step=((x/8)*7 + 3) % 5;
    int yy=y + step - 2;
    fb.fillRect(x,yy,8,CANOPY_BOTTOM-yy,C_CANOPY);
  }

  // massa verde clara inferior
  fb.fillRect(0,CANOPY_BOTTOM,160,GROUND_Y-CANOPY_BOTTOM,C_FIELD);
}

void drawTrunks(){
  // troncos finos e espaçados, como no original
  const int xs[] = {24, 68, 107, 145};
  for(uint8_t i=0;i<4;i++){
    fb.fillRect(xs[i],CANOPY_BOTTOM-1,4,GROUND_Y-(CANOPY_BOTTOM-1),C_TRUNK);
  }
}

void drawGround(){
  fb.fillRect(0,GROUND_Y,160,10,C_GROUND);
  fb.fillRect(0,GROUND_Y+10,160,10,C_GROUND2);
}

void drawLowerBand(){
  fb.fillRect(0,LOWER_BLACK_Y,160,20,ST77XX_BLACK);
  fb.fillRect(4,118,152,3,C_GROUND2);

  // contador/icone inferior simplificado
  fb.setTextSize(1);
  fb.setTextColor(ST77XX_WHITE);
  fb.setCursor(52,110);
  fb.print("x9");

  fb.setCursor(48,122);
  fb.print("ACTIVISION");
}

void drawForestScreen(){
  fb.fillScreen(ST77XX_BLACK);
  drawCanopy();
  drawTrunks();
  drawGround();
  drawHud();
}

void drawHarryPixel(int x,int feetY,bool swing,bool frameAlt){
  // sprite propositalmente pequeno e blocado, inspirado no original
  int y=feetY;

  // cabeca
  putPixelBlock(x, y-15, 3, 3, C_SKIN);
  putPixelBlock(x+2, y-16, 2, 2, ST77XX_BLACK);

  // camiseta rosada / pele
  putPixelBlock(x-1, y-12, 4, 5, C_SHIRT);
  putPixelBlock(x+3, y-11, 2, 2, C_SKIN);

  // calca verde
  putPixelBlock(x, y-7, 4, 4, C_PANTS);

  if(swing){
    fb.drawLine(x+1,y-10,x+6,y-16,C_SKIN);
    fb.drawLine(x+2,y-3,x-3,y+1,C_PANTS);
    fb.drawLine(x+3,y-3,x+8,y,C_PANTS);
  }else if(frameAlt){
    fb.drawLine(x,y-3,x-5,y+2,C_PANTS);
    fb.drawLine(x+3,y-3,x+7,y+1,C_PANTS);
    fb.drawLine(x,y-10,x-5,y-7,C_SKIN);
  }else{
    fb.drawLine(x,y-3,x-2,y+2,C_PANTS);
    fb.drawLine(x+3,y-3,x+8,y+2,C_PANTS);
    fb.drawLine(x+3,y-10,x+8,y-7,C_SKIN);
  }
}

void drawPit(int x,int w){
  fb.fillRect(x,GROUND_Y+2,w,6,ST77XX_BLACK);
  fb.fillRect(x+7,GROUND_Y,w-14,8,ST77XX_BLACK);
  fb.fillRect(x+15,GROUND_Y-2,w-30,3,ST77XX_BLACK);
}

void drawVineScene(){
  drawForestScreen();
  drawPit(54,58);

  // cipó preso no alto, com balanço bem visível
  int phase=(frameNo%120);
  int dx;
  if(phase<60) dx=-24 + (phase*48)/60;
  else dx=24 - ((phase-60)*48)/60;

  int endX=82+dx;
  int endY=66 + (abs(dx)*10)/24;
  fb.drawLine(82,45,endX,endY,ST77XX_BLACK);

  bool alt=((frameNo/4)&1);
  drawHarryPixel(endX-2,endY+15,true,alt);

  // escorpiao parado no lado direito, como na referencia
  drawScorpionAt(131,90);
}

void drawRollingLogScene(){
  drawForestScreen();

  // tronco rolando no solo
  int x=movingX;
  fb.fillRect(x,82,28,6,C_TRUNK);
  fb.fillRect(x+4,80,20,2,C_TRUNK);
  fb.drawRect(x+2,82,5,5,C_GROUND2);
  fb.drawRect(x+21,82,5,5,C_GROUND2);

  int heroX=34;
  int dist=x-heroX;
  int jump=0;
  if(dist<54 && dist>-18){
    int t=54-dist;
    // parabola inteira 0..72, evita float
    jump=(t*(72-t))/72;
    if(jump>18) jump=18;
  }
  drawHarryPixel(heroX,GROUND_Y-jump,false,((frameNo/3)&1));
}

void drawCrocAt(int x,bool open){
  // crocodilo estilo 2600: corpo horizontal e focinho pixelado
  fb.fillRect(x,91,15,3,C_CROC);
  fb.fillRect(x+3,88,8,3,C_CROC);
  fb.drawPixel(x+5,87,ST77XX_WHITE);
  if(open){
    fb.drawLine(x+12,90,x+18,87,C_CROC);
    fb.drawLine(x+12,94,x+18,97,C_CROC);
  }else{
    fb.drawFastHLine(x+11,92,8,C_CROC);
  }
}

void drawCrocsScene(){
  drawForestScreen();
  drawPit(47,70);
  for(int i=0;i<3;i++) drawCrocAt(55+i*22,((frameNo/8+i)&1)==0);

  int phase=frameNo%150;
  int hx=27;
  int jump=0;
  if(phase>30 && phase<115){
    int t=phase-30;
    hx=27+(t*105)/85;
    jump=(t*(85-t))/95;
    if(jump>22) jump=22;
  }
  drawHarryPixel(hx,GROUND_Y-jump,false,((frameNo/3)&1));
}

void drawScorpionAt(int x,int y){
  // mais proximo do pequeno escorpiao branco do original
  fb.fillRect(x,y-3,6,3,C_SCORPION);
  fb.fillRect(x+5,y-5,3,2,C_SCORPION);
  fb.drawLine(x+7,y-5,x+10,y-9,C_SCORPION);
  fb.drawLine(x+10,y-9,x+8,y-12,C_SCORPION);
  fb.drawLine(x-1,y,x-4,y+2,C_SCORPION);
  fb.drawLine(x+2,y,x+5,y+2,C_SCORPION);
}

void drawScorpionScene(){
  drawForestScreen();
  drawScorpionAt(movingX,91);

  int heroX=35;
  int dist=movingX-heroX;
  int jump=0;
  if(dist<45 && dist>-12){
    int t=45-dist;
    jump=(t*(57-t))/85;
    if(jump>15) jump=15;
  }
  drawHarryPixel(heroX,GROUND_Y-jump,false,((frameNo/3)&1));
}

void drawUndergroundScene(){
  fb.fillScreen(ST77XX_BLACK);

  // topo continua com HUD verde, como as transicoes subterraneas do jogo
  fb.fillRect(0,0,160,31,C_SKY);
  drawHud();

  // corredor subterraneo em tons terrosos
  fb.fillRect(0,34,160,14,C_GROUND2);
  fb.fillRect(0,88,160,15,C_TRUNK);

  // colunas/tijolos pixelados
  for(int x=8;x<160;x+=30){
    fb.fillRect(x,48,4,40,C_DARKBROWN());
  }

  // tesouros rolando para a esquerda
  int scroll=(frameNo*2)%70;
  for(int x=-20;x<200;x+=70){
    int xx=x-scroll;
    fb.fillRect(xx,78,10,8,C_GOLD);
    fb.drawRect(xx+2,76,6,2,ST77XX_WHITE);
  }

  drawHarryPixel(36,87,false,((frameNo/3)&1));
}

uint16_t C_DARKBROWN(){ return tft.color565(92,48,6); }

void updateScene(){
  unsigned long now=millis();
  if(now-sceneStart>=9000UL){
    scene=(SceneType)(((uint8_t)scene+1)%5);
    sceneStart=now;
    movingX=182;
  }
}

void updateMovingObjects(){
  if(scene==SCENE_LOG || scene==SCENE_SCORPION){
    movingX-=3;
    if(movingX<-35) movingX=182;
  }
}

void renderFrame(){
  switch(scene){
    case SCENE_VINE: drawVineScene(); break;
    case SCENE_LOG: drawRollingLogScene(); break;
    case SCENE_CROCS: drawCrocsScene(); break;
    case SCENE_SCORPION: drawScorpionScene(); break;
    case SCENE_UNDERGROUND: drawUndergroundScene(); break;
  }

  drawLowerBand();
  tft.drawRGBBitmap(0,0,fb.getBuffer(),160,128);
}

void setup(){
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  C_SKY=tft.color565(0,95,12);
  C_CANOPY=tft.color565(0,112,12);
  C_FIELD=tft.color565(105,226,12);
  C_TRUNK=tft.color565(102,48,5);
  C_GROUND=tft.color565(255,224,50);
  C_GROUND2=tft.color565(154,118,0);
  C_SKIN=tft.color565(247,177,125);
  C_SHIRT=tft.color565(246,92,175);
  C_PANTS=tft.color565(20,88,45);
  C_SCORPION=tft.color565(235,235,225);
  C_CROC=tft.color565(45,116,35);
  C_GOLD=tft.color565(255,194,20);

  lastFrame=sceneStart=millis();
}

void loop(){
  unsigned long now=millis();
  updateScene();

  if(now-lastFrame>=FRAME_MS){
    lastFrame=now;
    frameNo++;
    updateMovingObjects();
    renderFrame();
  }
  yield();
}

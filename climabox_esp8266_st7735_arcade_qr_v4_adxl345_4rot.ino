#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Wire.h>
extern "C" {
  #include <user_interface.h>
}

#define TFT_CS D8
#define TFT_RST D1
#define TFT_DC D2
#define CONFIG_BUTTON 0

// ADXL345: SDA -> D3/GPIO0, SCL -> D4/GPIO2, CS -> 3V3, SDO -> GND.
#define ADXL_SDA D3
#define ADXL_SCL D4
#define ADXL_ADDR 0x53

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long WEATHER_RETRY_INTERVAL = 15000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const unsigned long GAME_FRAME_INTERVAL = 65UL;
const unsigned long ADXL_INTERVAL = 100UL;
const unsigned long ORIENTATION_STABLE_MS = 650UL;
const int16_t ORIENTATION_THRESHOLD = 140;

const char* QR_WIFI[29] = {
  "11111110000110100101001111111","10000010011001100110101000001","10111010000010100000101011101",
  "10111010100011001101101011101","10111010110001101011001011101","10000010000110001000001000001",
  "11111110101010101010101111111","00000000001100000000000000000","11000111010101010101100011000",
  "11101001010011010001010010001","10110010001001011000111101000","01001101010010101011101010001",
  "11000010010110101000101010001","01010000110001111001100110100","11100111110111011000010100100",
  "10101101111100001001110001111","00101111000100000100000101011","11000101100011011000101011000",
  "11010011110100011111000011101","10001001111011001010000010011","10110011111010100110111111100",
  "00000000101000110100100010011","11111110100110011111101010100","10000010101100110100100010011",
  "10111010010001111101111110110","10111010010011111001010011001","10111010001001110011111111110",
  "10000010100011111010000011101","11111110110010101111000011000"
};

String cidade="Manaus", uf="AM";
float latitude=-3.1190f, longitude=-60.0217f;
bool coordenadasValidas=true;
float temperatura=NAN, sensacao=NAN, umidade=NAN, vento=NAN;
int weatherCode=-1;
bool dadosValidos=false;
unsigned long ultimaConsulta=0, ultimaTentativaClima=0, ultimoFrameJogo=0;

enum TelaModo:uint32_t { MODO_CLIMA=0, MODO_PACMAN=1, MODO_PITFALL=2, MODO_ENDURO=3, MODO_RIVER=4 };
const uint32_t TOTAL_MODOS=5;
struct EstadoRtc { uint32_t magic, modo, checksum; };
const uint32_t RTC_MAGIC=0x434C494D, RTC_SLOT=64;
TelaModo modoAtual=MODO_CLIMA;

bool adxlOk=false;
int16_t accelX=0, accelY=0, accelZ=0;
uint8_t rotacaoAtual=1;
uint8_t rotacaoCandidata=1;
unsigned long ultimaLeituraAdxl=0;
unsigned long inicioRotacaoCandidata=0;

bool modoRetrato(){ return rotacaoAtual==0 || rotacaoAtual==2; }

uint32_t checksumRtc(const EstadoRtc& e){ return e.magic ^ e.modo ^ 0xA55AA55A; }
bool lerEstadoRtc(EstadoRtc& e){ return system_rtc_mem_read(RTC_SLOT,&e,sizeof(e)) && e.magic==RTC_MAGIC && e.checksum==checksumRtc(e) && e.modo<TOTAL_MODOS; }
void salvarModoRtc(TelaModo m){ EstadoRtc e{RTC_MAGIC,(uint32_t)m,0}; e.checksum=checksumRtc(e); system_rtc_mem_write(RTC_SLOT,&e,sizeof(e)); }
void determinarModoInicial(){
  const rst_info* info=ESP.getResetInfoPtr(); EstadoRtc e; bool ok=lerEstadoRtc(e);
  if(!info || info->reason==REASON_DEFAULT_RST || !ok){ modoAtual=MODO_CLIMA; salvarModoRtc(modoAtual); return; }
  if(info->reason==REASON_EXT_SYS_RST){ modoAtual=(TelaModo)((e.modo+1)%TOTAL_MODOS); salvarModoRtc(modoAtual); return; }
  modoAtual=(TelaModo)e.modo;
}

void telaLimpa(uint16_t c=ST77XX_BLACK){ tft.fillScreen(c); }
void textoLinha(int y,const String& s,uint16_t c=ST77XX_WHITE,uint8_t tam=1){ tft.setTextColor(c); tft.setTextSize(tam); tft.setCursor(5,y); tft.print(s); }
void rodape(const String& s){
  int y=tft.height()-12;
  tft.fillRect(0,y,tft.width(),12,ST77XX_BLACK);
  tft.setTextColor(tft.color565(100,100,100));tft.setTextSize(1);tft.setCursor(5,y+3);tft.print("RST: ");tft.print(s);
}

bool adxlWrite(uint8_t reg,uint8_t value){ Wire.beginTransmission(ADXL_ADDR);Wire.write(reg);Wire.write(value);return Wire.endTransmission()==0; }
bool adxlReadBytes(uint8_t reg,uint8_t* buf,uint8_t n){ Wire.beginTransmission(ADXL_ADDR);Wire.write(reg);if(Wire.endTransmission(false)!=0)return false;uint8_t got=Wire.requestFrom((uint8_t)ADXL_ADDR,n);if(got!=n)return false;for(uint8_t i=0;i<n;i++)buf[i]=Wire.read();return true; }
bool iniciarADXL345(){
  Wire.begin(ADXL_SDA,ADXL_SCL);Wire.setClock(100000);delay(10);
  uint8_t id=0;if(!adxlReadBytes(0x00,&id,1)||id!=0xE5)return false;
  adxlWrite(0x2D,0x00);adxlWrite(0x31,0x08);adxlWrite(0x2C,0x0A);adxlWrite(0x2D,0x08);delay(10);return true;
}
bool lerADXL345(){ uint8_t b[6];if(!adxlReadBytes(0x32,b,6))return false;accelX=(int16_t)((uint16_t)b[1]<<8|b[0]);accelY=(int16_t)((uint16_t)b[3]<<8|b[2]);accelZ=(int16_t)((uint16_t)b[5]<<8|b[4]);return true; }

void prepararModo();
void aplicarRotacao(uint8_t nova){ if(nova==rotacaoAtual)return;rotacaoAtual=nova;tft.setRotation(rotacaoAtual);telaLimpa();prepararModo();Serial.print("Rotacao TFT: ");Serial.println(rotacaoAtual); }

void atualizarOrientacao(){
  if(!adxlOk)return;
  unsigned long agora=millis();if(agora-ultimaLeituraAdxl<ADXL_INTERVAL)return;ultimaLeituraAdxl=agora;
  if(!lerADXL345())return;

  uint8_t desejada=rotacaoAtual;
  int16_t ax=abs(accelX), ay=abs(accelY), az=abs(accelZ);

  // Considera os tres eixos. X/Y mantem exatamente o comportamento anterior.
  // Quando Z domina, o ADXL345 esta na base/topo do cubo. Nessa montagem
  // a tela precisa ficar em retrato; cada sinal de Z representa uma das duas faces.
  if(ax>=ay && ax>=az && ax>ORIENTATION_THRESHOLD){
    desejada=(accelX>0)?0:2;
  } else if(ay>=ax && ay>=az && ay>ORIENTATION_THRESHOLD){
    desejada=(accelY>0)?1:3;
  } else if(az>=ax && az>=ay && az>ORIENTATION_THRESHOLD){
    desejada=(accelZ>0)?0:2;
  } else return;

  if(desejada!=rotacaoCandidata){rotacaoCandidata=desejada;inicioRotacaoCandidata=agora;return;}
  if(desejada!=rotacaoAtual && agora-inicioRotacaoCandidata>=ORIENTATION_STABLE_MS)aplicarRotacao(desejada);
}

bool salvarConfig(){ DynamicJsonDocument d(384);d["cidade"]=cidade;d["uf"]=uf;d["latitude"]=latitude;d["longitude"]=longitude;d["coords_ok"]=coordenadasValidas;File f=LittleFS.open(CONFIG_FILE,"w");if(!f)return false;serializeJson(d,f);f.close();return true; }
void carregarConfig(){ if(!LittleFS.exists(CONFIG_FILE))return;File f=LittleFS.open(CONFIG_FILE,"r");if(!f)return;DynamicJsonDocument d(384);if(!deserializeJson(d,f)){cidade=d["cidade"]|"Manaus";uf=d["uf"]|"AM";latitude=d["latitude"]|-3.1190f;longitude=d["longitude"]|-60.0217f;coordenadasValidas=d["coords_ok"]|true;}f.close(); }
String urlEncode(const String& v){String s;char h[4];for(size_t i=0;i<v.length();i++){uint8_t c=v[i];if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')s+=(char)c;else{snprintf(h,sizeof(h),"%%%02X",c);s+=h;}}return s;}

void desenharQrWifi(){
  int e=modoRetrato()?3:3,q=2;
  int t=(29+q*2)*e;
  int ox=(tft.width()-t)/2;
  int oy=modoRetrato()?8:14;
  tft.fillRect(ox,oy,t,t,ST77XX_WHITE);
  for(int y=0;y<29;y++)for(int x=0;x<29;x++)if(QR_WIFI[y][x]=='1')tft.fillRect(ox+(x+q)*e,oy+(y+q)*e,e,e,ST77XX_BLACK);
}
void mostrarPortalNaTela(){
  telaLimpa();desenharQrWifi();tft.setTextWrap(false);tft.setTextSize(1);
  if(modoRetrato()){
    tft.setTextColor(ST77XX_YELLOW);tft.setCursor(6,112);tft.print("CONFIG ClimaBox");
    tft.setTextColor(ST77XX_WHITE);tft.setCursor(6,124);tft.print("Wi-Fi: ClimaBox-Setup");
    tft.setCursor(6,136);tft.print("Senha: climabox");
    tft.setCursor(6,148);tft.print("IP: 192.168.4.1");
  } else {
    tft.setTextColor(ST77XX_YELLOW);tft.setCursor(104,16);tft.print("CONFIG");
    tft.setTextColor(ST77XX_CYAN);tft.setCursor(104,34);tft.print("ClimaBox");tft.setCursor(104,45);tft.print("-Setup");
    tft.setTextColor(ST77XX_YELLOW);tft.setCursor(104,65);tft.print("Senha");tft.setTextColor(ST77XX_WHITE);tft.setCursor(104,78);tft.print("climabox");tft.setCursor(104,100);tft.print("192.168");tft.setCursor(104,111);tft.print(".4.1");
  }
}
void callbackAP(WiFiManager*){ mostrarPortalNaTela(); }

bool geocodificarCidade(){
  if(WiFi.status()!=WL_CONNECTED)return false;String busca=cidade;if(uf.length())busca+=", "+uf;
  BearSSL::WiFiClientSecure client;client.setInsecure();client.setBufferSizes(2048,512);
  HTTPClient http;String url="https://geocoding-api.open-meteo.com/v1/search?name="+urlEncode(busca)+"&count=1&language=pt&format=json&countryCode=BR";
  if(!http.begin(client,url))return false;http.setTimeout(10000);http.useHTTP10(true);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}
  DynamicJsonDocument d(2048);DeserializationError er=deserializeJson(d,http.getStream());http.end();if(er)return false;JsonArray r=d["results"].as<JsonArray>();if(r.isNull()||r.size()==0)return false;
  latitude=r[0]["latitude"].as<float>();longitude=r[0]["longitude"].as<float>();const char* n=r[0]["name"]|nullptr;if(n&&strlen(n))cidade=n;coordenadasValidas=true;salvarConfig();return true;
}

bool abrirPortalConfiguracao(){
  WiFiManager wm;wm.setAPCallback(callbackAP);wm.setConfigPortalTimeout(0);wm.setMinimumSignalQuality(8);wm.setRemoveDuplicateAPs(true);
  char c[41]={0},u[5]={0};cidade.toCharArray(c,sizeof(c));uf.toCharArray(u,sizeof(u));
  WiFiManagerParameter p0("<p><strong>Local do clima</strong></p>"),p1("cidade","Cidade",c,40),p2("uf","UF",u,4);wm.addParameter(&p0);wm.addParameter(&p1);wm.addParameter(&p2);
  mostrarPortalNaTela();if(!wm.startConfigPortal(AP_SSID,AP_PASSWORD)||WiFi.status()!=WL_CONNECTED)return false;
  cidade=p1.getValue();uf=p2.getValue();cidade.trim();uf.trim();uf.toUpperCase();coordenadasValidas=false;salvarConfig();return geocodificarCidade();
}

bool conectarWifi(bool esperar){ WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.persistent(true);if(!WiFi.SSID().length())return abrirPortalConfiguracao();WiFi.begin();if(!esperar)return true;unsigned long ini=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-ini<WIFI_CONNECT_TIMEOUT){delay(100);yield();}return WiFi.status()==WL_CONNECTED; }

String descricaoTempo(int c){if(c==0)return"Ceu limpo";if(c<=2)return"Parc. nublado";if(c==3)return"Nublado";if(c==45||c==48)return"Neblina";if(c>=51&&c<=57)return"Garoa";if(c>=61&&c<=67)return"Chuva";if(c>=80&&c<=82)return"Pancadas";if(c>=95)return"Trovoadas";return"Variavel";}
void desenharIconeClima(int code,int cx,int cy){uint16_t am=ST77XX_YELLOW,br=ST77XX_WHITE;if(code<=1){tft.fillCircle(cx,cy,9,am);for(int a=0;a<8;a++){float ang=a*PI/4;tft.drawLine(cx+cos(ang)*12,cy+sin(ang)*12,cx+cos(ang)*17,cy+sin(ang)*17,am);}}else{tft.fillCircle(cx-8,cy+2,7,br);tft.fillCircle(cx+1,cy-4,10,br);tft.fillCircle(cx+11,cy+3,7,br);tft.fillRect(cx-14,cy+2,32,9,br);}}

void desenharTelaClima(){
  telaLimpa();String local=cidade+(uf.length()?"-"+uf:"");
  if(modoRetrato()){
    if(local.length()>14)local=local.substring(0,14);
    tft.setTextColor(ST77XX_CYAN);tft.setTextSize(2);tft.setCursor(5,5);tft.print(local);
    desenharIconeClima(weatherCode,64,48);
    tft.setTextColor(ST77XX_YELLOW);tft.setTextSize(4);tft.setCursor(28,72);if(dadosValidos){tft.print(temperatura,0);tft.print("C");}else tft.print("--C");
    tft.setTextColor(ST77XX_WHITE);tft.setTextSize(1);tft.setCursor(5,108);tft.print(dadosValidos?descricaoTempo(weatherCode):"Aguardando Wi-Fi...");
    tft.setCursor(5,123);tft.print("Sens: ");if(dadosValidos)tft.print(sensacao,0);else tft.print("--");tft.print(" C");
    tft.setCursor(5,136);tft.print("Umid: ");if(dadosValidos)tft.print(umidade,0);else tft.print("--");tft.print(" %");
    tft.setCursor(5,149);tft.print("Vento: ");if(dadosValidos)tft.print(vento,0);else tft.print("--");tft.print(" km/h");
  } else {
    if(local.length()>17)local=local.substring(0,17);textoLinha(3,local,ST77XX_CYAN,2);desenharIconeClima(weatherCode,27,48);
    tft.setTextColor(ST77XX_YELLOW);tft.setTextSize(4);tft.setCursor(54,34);if(dadosValidos){tft.print(temperatura,0);tft.print("C");}else tft.print("--C");
    textoLinha(76,dadosValidos?descricaoTempo(weatherCode):"Aguardando Wi-Fi...");tft.setCursor(5,91);tft.print("Sens: ");if(dadosValidos)tft.print(sensacao,0);else tft.print("--");tft.print("C Umid: ");if(dadosValidos)tft.print(umidade,0);else tft.print("--");tft.print("%");tft.setCursor(5,106);tft.print("Vento: ");if(dadosValidos)tft.print(vento,0);else tft.print("--");tft.print(" km/h");
  }
  rodape("Pac-Man");
}

bool buscarClima(){
  ultimaTentativaClima=millis();if(WiFi.status()!=WL_CONNECTED)return false;if(!coordenadasValidas&&!geocodificarCidade())return false;
  BearSSL::WiFiClientSecure client;client.setInsecure();client.setBufferSizes(2048,512);HTTPClient http;
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(latitude,5)+"&longitude="+String(longitude,5)+"&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code&timezone=auto";
  if(!http.begin(client,url))return false;http.setTimeout(10000);http.useHTTP10(true);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}
  DynamicJsonDocument d(2048);DeserializationError er=deserializeJson(d,http.getStream());http.end();if(er)return false;
  temperatura=d["current"]["temperature_2m"]|NAN;sensacao=d["current"]["apparent_temperature"]|NAN;umidade=d["current"]["relative_humidity_2m"]|NAN;vento=d["current"]["wind_speed_10m"]|NAN;weatherCode=d["current"]["weather_code"]|-1;dadosValidos=!isnan(temperatura);if(dadosValidos)ultimaConsulta=millis();if(modoAtual==MODO_CLIMA)desenharTelaClima();return dadosValidos;
}

GFXcanvas16 cena(160,40);
uint32_t frameJogo=0;unsigned long inicioCena=0;

void desenharCanvasJogo(){
  if(!modoRetrato()){
    tft.drawRGBBitmap(0,43,cena.getBuffer(),160,40);
  } else {
    // Em retrato mostramos a faixa central de 128 px sem reservar outro framebuffer.
    // Isso preserva RAM; cada linha usa o stride original de 160 pixels.
    int y0=58;
    uint16_t* p=cena.getBuffer();
    for(int y=0;y<40;y++)tft.drawRGBBitmap(0,y0+y,p+y*160+16,128,1);
  }
}

void tituloJogo(const String& t,uint16_t c,const String& prox){
  telaLimpa();tft.setTextWrap(false);tft.setTextColor(c);tft.setTextSize(modoRetrato()?2:1);tft.setCursor(5,modoRetrato()?8:5);tft.print(t);rodape(prox);
}
void ghost(int x,int y,uint16_t cor,bool medo=false){cena.fillCircle(x,y-3,8,cor);cena.fillRect(x-8,y-3,16,11,cor);cena.fillCircle(x-3,y-4,3,ST77XX_WHITE);cena.fillCircle(x+3,y-4,3,ST77XX_WHITE);uint16_t p=medo?ST77XX_WHITE:tft.color565(30,70,255);cena.fillCircle(x-2,y-4,1,p);cena.fillCircle(x+4,y-4,1,p);}
void pac(int x,int y,bool b){cena.fillCircle(x,y,9,ST77XX_YELLOW);if(b)cena.fillTriangle(x+1,y,x+11,y-6,x+11,y+6,ST77XX_BLACK);else cena.drawFastHLine(x+2,y,8,ST77XX_BLACK);}
void prepararPacman(){tituloJogo("PAC-MAN",ST77XX_YELLOW,"Pitfall");frameJogo=0;inicioCena=millis();}
void atualizarPacman(){bool cac=((millis()-inicioCena)/6500UL)%2;cena.fillScreen(ST77XX_BLACK);for(int x=-16;x<176;x+=16)cena.fillCircle(x-((frameJogo*2)%16),20,2,ST77XX_WHITE);bool b=((frameJogo/3)%2)==0;if(!cac){pac(128,20,b);ghost(91,20,tft.color565(70,220,255));ghost(64,20,tft.color565(255,120,210));ghost(37,20,tft.color565(255,70,70));}else{uint16_t m=tft.color565(45,65,235);pac(32,20,b);ghost(72,20,m,true);ghost(101,20,m,true);ghost(130,20,m,true);}desenharCanvasJogo();}

void prepararPitfall(){tituloJogo("PITFALL",tft.color565(70,220,80),"Enduro");frameJogo=0;}
void atualizarPitfall(){cena.fillScreen(tft.color565(75,175,210));cena.fillRect(0,27,160,13,tft.color565(145,90,35));int d=(frameJogo*2)%48;for(int x=-24;x<190;x+=48){int xx=x-d;cena.fillRect(xx+12,5,5,24,tft.color565(105,60,25));cena.fillCircle(xx+14,4,10,tft.color565(20,105,30));}int obst=180-((frameJogo*3)%210);cena.fillRoundRect(obst,25,24,6,3,tft.color565(95,45,15));int hy=(obst>28&&obst<78)?12:26;cena.fillCircle(37,hy-9,3,tft.color565(235,175,110));cena.fillRect(34,hy-6,6,8,tft.color565(245,225,80));cena.drawLine(37,hy,31,hy+7,ST77XX_BLACK);cena.drawLine(39,hy,45,hy+7,ST77XX_BLACK);desenharCanvasJogo();}

void prepararEnduro(){tituloJogo("ENDURO",ST77XX_WHITE,"River Rider");frameJogo=0;}
void carro(int x,int y,uint16_t c,int e){cena.fillRect(x-5*e,y-4*e,10*e,4*e,c);cena.fillRect(x-3*e,y-6*e,6*e,2*e,c);}
void atualizarEnduro(){uint16_t ceu=tft.color565(85,150,220),gr=tft.color565(45,140,45),as=tft.color565(65,65,68);cena.fillScreen(ceu);cena.fillRect(0,13,160,27,gr);cena.fillTriangle(78,12,28,40,132,40,as);for(int y=16+((frameJogo*2)%12);y<40;y+=12)cena.fillRect(79,y,2,5,ST77XX_YELLOW);carro(80,38,ST77XX_WHITE,2);int ey=14+((frameJogo*2)%28);carro(70+((frameJogo/30)%3)*10,ey,tft.color565(230,55,55),1);desenharCanvasJogo();}

void prepararRiver(){tituloJogo("RIVER RIDER",tft.color565(80,180,255),"Clima");frameJogo=0;}
void atualizarRiver(){uint16_t agua=tft.color565(25,90,205),m=tft.color565(55,150,55);cena.fillScreen(m);int sc=(frameJogo*2)%80;for(int y=0;y<40;y++){int centro=80+(int)(sin((y+sc)*0.12f)*15);int meia=32;cena.drawFastHLine(centro-meia,y,meia*2,agua);}int px=80+(int)(sin(frameJogo*0.08f)*9);cena.fillTriangle(px,31,px-4,39,px+4,39,ST77XX_WHITE);cena.fillRect(px-10,34,20,3,ST77XX_WHITE);int ey=(frameJogo*2)%50-5;if(ey>=0&&ey<35)cena.fillTriangle(80,ey+6,74,ey-4,86,ey-4,tft.color565(230,60,50));desenharCanvasJogo();}

void prepararModo(){ultimoFrameJogo=millis();switch(modoAtual){case MODO_CLIMA:desenharTelaClima();break;case MODO_PACMAN:prepararPacman();break;case MODO_PITFALL:prepararPitfall();break;case MODO_ENDURO:prepararEnduro();break;case MODO_RIVER:prepararRiver();break;}}
void atualizarJogo(){if(modoAtual==MODO_CLIMA)return;unsigned long a=millis();if(a-ultimoFrameJogo<GAME_FRAME_INTERVAL)return;ultimoFrameJogo=a;frameJogo++;switch(modoAtual){case MODO_PACMAN:atualizarPacman();break;case MODO_PITFALL:atualizarPitfall();break;case MODO_ENDURO:atualizarEnduro();break;case MODO_RIVER:atualizarRiver();break;default:break;}}

void verificarBotaoConfig(){if(digitalRead(CONFIG_BUTTON)!=LOW)return;unsigned long ini=millis();while(digitalRead(CONFIG_BUTTON)==LOW){if(millis()-ini>=BUTTON_HOLD_TIME){abrirPortalConfiguracao();ESP.restart();return;}delay(20);yield();}}

void setup(){
  Serial.begin(115200);pinMode(CONFIG_BUTTON,INPUT_PULLUP);tft.initR(INITR_BLACKTAB);tft.setRotation(rotacaoAtual);telaLimpa();determinarModoInicial();LittleFS.begin();carregarConfig();
  adxlOk=iniciarADXL345();Serial.println(adxlOk?"ADXL345 OK - 6 posicoes":"ADXL345 nao encontrado - orientacao automatica desativada");
  if(adxlOk && lerADXL345()){
    // Determina a orientacao inicial antes de desenhar a interface, inclusive
    // quando o eixo Z esta apontando para cima/baixo (sensor na base/topo do cubo).
    int16_t ax=abs(accelX),ay=abs(accelY),az=abs(accelZ);
    if(ax>=ay && ax>=az && ax>ORIENTATION_THRESHOLD)rotacaoAtual=(accelX>0)?0:2;
    else if(ay>=ax && ay>=az && ay>ORIENTATION_THRESHOLD)rotacaoAtual=(accelY>0)?1:3;
    else if(az>=ax && az>=ay && az>ORIENTATION_THRESHOLD)rotacaoAtual=(accelZ>0)?0:2;
    rotacaoCandidata=rotacaoAtual;tft.setRotation(rotacaoAtual);
  }
  bool ok=conectarWifi(modoAtual==MODO_CLIMA);if(modoAtual==MODO_CLIMA&&ok)buscarClima();prepararModo();
}

void loop(){
  verificarBotaoConfig();atualizarOrientacao();unsigned long a=millis();
  if(modoAtual==MODO_CLIMA){
    if(WiFi.status()!=WL_CONNECTED&&a-ultimaTentativaClima>=WEATHER_RETRY_INTERVAL){ultimaTentativaClima=a;WiFi.disconnect();delay(20);WiFi.begin();}
    if(WiFi.status()==WL_CONNECTED&&((!dadosValidos&&a-ultimaTentativaClima>=WEATHER_RETRY_INTERVAL)||(dadosValidos&&a-ultimaConsulta>=WEATHER_INTERVAL)))buscarClima();
  }
  atualizarJogo();yield();
}

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

#define TFT_CS   D8
#define TFT_RST  D1
#define TFT_DC   D2
#define CONFIG_BUTTON 0

// ADXL345: SDA D3/GPIO0, SCL D4/GPIO2, CS 3V3, SDO GND.
#define ADXL_SDA  D3
#define ADXL_SCL  D4
#define ADXL_ADDR 0x53

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long SCREEN_INTERVAL = 20UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const unsigned long ADXL_INTERVAL = 100UL;
const unsigned long ORIENTATION_STABLE_MS = 650UL;
const int16_t ORIENTATION_THRESHOLD = 135;
const uint8_t TOTAL_SCREENS = 4;

String cidade = "Manaus", uf = "AM";
float latitude = -3.1190f, longitude = -60.0217f;
bool coordenadasValidas = true;

float temperatura=NAN, sensacao=NAN, umidade=NAN, vento=NAN, rajada=NAN;
float pressao=NAN, precipitacao=NAN, nuvens=NAN, visibilidade=NAN;
int weatherCode=-1; bool isDay=true; String weatherTime="";
float aqi=NAN, pm25=NAN, pm10=NAN, co=NAN, no2=NAN, so2=NAN;
float ozonio=NAN, aerosol=NAN, poeira=NAN, uv=NAN; String airTime="";

unsigned long ultimaConsulta=0, ultimaTrocaTela=0;
unsigned long ultimaTentativaWifi=0;
uint8_t telaAtual=0;

bool adxlOk=false;
int16_t accelX=0, accelY=0, accelZ=0;
uint8_t rotacaoAtual=1, rotacaoCandidata=1;
unsigned long ultimaLeituraAdxl=0, inicioRotacaoCandidata=0;

// QR estatico para WIFI:T:WPA;S:ClimaBox-Setup;P:climabox;;
const char* QR_WIFI[29] = {
"11111110000110100101001111111","10000010011001100110101000001","10111010000010100000101011101",
"10111010100011001101101011101","10111010110001101011001011101","10000010000110001000001000001",
"11111110101010101010101111111","00000000001100000000000000000","11000111010101010101100011000",
"11101001010011010001010010001","10110010001001011000111101000","01001101010010101011101010001",
"11000010010110101000101010001","01010000110001111001100110100","11100111110111011000010100100",
"10101101111100001001110001111","00101111000100000100000101011","11000101100011011000101011000",
"11010011110100011111000011101","10001001111011001010000010011","10110011111010100110111111100",
"00000000101000110100100010011","11111110100110011111101010100","10000010101100110001100011000",
"10111010010001111101111110110","10111010010011111001010011001","10111010001001110011111111110",
"10000010100011111010000011101","11111110110010101111000011000"};

bool retrato(){ return rotacaoAtual==0 || rotacaoAtual==2; }
int W(){ return tft.width(); }
int H(){ return tft.height(); }

void telaLimpa(uint16_t c=ST77XX_BLACK){ tft.fillScreen(c); }
void texto(int x,int y,const String& s,uint16_t c=ST77XX_WHITE,uint8_t z=1){ tft.setTextWrap(false);tft.setTextColor(c);tft.setTextSize(z);tft.setCursor(x,y);tft.print(s); }
void cabecalho(const String& s,uint16_t c=ST77XX_CYAN){ telaLimpa();texto(5,5,s,c,retrato()?1:2);int yy=retrato()?19:25;tft.drawFastHLine(0,yy,W(),tft.color565(70,70,70)); }
String horaISO(const String& iso){ return iso.length()>=16?iso.substring(11,16):"--:--"; }
void rodape(const String& h){ int y=H()-10;tft.fillRect(0,y-2,W(),12,ST77XX_BLACK);texto(5,y,"Atualizado "+(h.length()?h:"--:--"),tft.color565(120,120,120)); }
void valorLinha(int y,const String& r,float v,const String& u,uint16_t c=ST77XX_WHITE,uint8_t casas=0){tft.setTextColor(c);tft.setTextSize(1);tft.setCursor(5,y);tft.print(r);if(isnan(v))tft.print("--");else{tft.print(v,casas);tft.print(u);}}

bool adxlWrite(uint8_t r,uint8_t v){Wire.beginTransmission(ADXL_ADDR);Wire.write(r);Wire.write(v);return Wire.endTransmission()==0;}
bool adxlRead(uint8_t r,uint8_t* b,uint8_t n){Wire.beginTransmission(ADXL_ADDR);Wire.write(r);if(Wire.endTransmission(false)!=0)return false;uint8_t g=Wire.requestFrom((uint8_t)ADXL_ADDR,n);if(g!=n)return false;for(uint8_t i=0;i<n;i++)b[i]=Wire.read();return true;}
bool iniciarADXL(){Wire.begin(ADXL_SDA,ADXL_SCL);Wire.setClock(100000);delay(10);uint8_t id=0;if(!adxlRead(0x00,&id,1)||id!=0xE5)return false;adxlWrite(0x2D,0);adxlWrite(0x31,0x08);adxlWrite(0x2C,0x0A);adxlWrite(0x2D,0x08);delay(10);return true;}
bool lerADXL(){uint8_t b[6];if(!adxlRead(0x32,b,6))return false;accelX=(int16_t)(((uint16_t)b[1]<<8)|b[0]);accelY=(int16_t)(((uint16_t)b[3]<<8)|b[2]);accelZ=(int16_t)(((uint16_t)b[5]<<8)|b[4]);return true;}

void mostrarTelaAtual();
void aplicarRotacao(uint8_t r){if(r==rotacaoAtual)return;rotacaoAtual=r;tft.setRotation(r);telaLimpa();mostrarTelaAtual();}
void atualizarOrientacao(){
  if(!adxlOk)return;unsigned long a=millis();if(a-ultimaLeituraAdxl<ADXL_INTERVAL)return;ultimaLeituraAdxl=a;if(!lerADXL())return;
  uint8_t d=rotacaoAtual;int ax=abs(accelX), ay=abs(accelY);
  if(ax<ORIENTATION_THRESHOLD && ay<ORIENTATION_THRESHOLD)return;
  if(ax>ay){ d=(accelX>0)?0:2; } else { d=(accelY>0)?1:3; }
  if(d!=rotacaoCandidata){rotacaoCandidata=d;inicioRotacaoCandidata=a;return;}
  if(d!=rotacaoAtual && a-inicioRotacaoCandidata>=ORIENTATION_STABLE_MS)aplicarRotacao(d);
}

bool salvarConfig(){DynamicJsonDocument d(384);d["cidade"]=cidade;d["uf"]=uf;d["latitude"]=latitude;d["longitude"]=longitude;d["coords_ok"]=coordenadasValidas;File f=LittleFS.open(CONFIG_FILE,"w");if(!f)return false;serializeJson(d,f);f.close();return true;}
void carregarConfig(){if(!LittleFS.exists(CONFIG_FILE))return;File f=LittleFS.open(CONFIG_FILE,"r");if(!f)return;DynamicJsonDocument d(384);if(!deserializeJson(d,f)){cidade=d["cidade"]|"Manaus";uf=d["uf"]|"AM";latitude=d["latitude"]|-3.119f;longitude=d["longitude"]|-60.0217f;coordenadasValidas=d["coords_ok"]|true;}f.close();}
String urlEncode(const String& v){String s;char h[4];for(size_t i=0;i<v.length();i++){uint8_t c=v[i];if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')s+=(char)c;else{snprintf(h,sizeof(h),"%%%02X",c);s+=h;}}return s;}

void desenharQrWifi(){int e=retrato()?2:3,q=2,t=(29+q*2)*e;int ox=4,oy=4;tft.fillRect(ox,oy,t,t,ST77XX_WHITE);for(int y=0;y<29;y++)for(int x=0;x<29;x++)if(QR_WIFI[y][x]=='1')tft.fillRect(ox+(x+q)*e,oy+(y+q)*e,e,e,ST77XX_BLACK);}
void mostrarPortalNaTela(){telaLimpa();desenharQrWifi();if(retrato()){texto(5,80,"CONFIG",ST77XX_CYAN);texto(5,94,"WiFi: ClimaBox-Setup");texto(5,108,"Senha: climabox",ST77XX_YELLOW);texto(5,122,"IP: 192.168.4.1",ST77XX_GREEN);}else{texto(108,8,"CONFIG",ST77XX_CYAN);texto(108,28,"Leia QR");texto(108,50,"Senha:",ST77XX_YELLOW);texto(108,62,"climabox");texto(108,82,"192.168",ST77XX_GREEN);texto(108,94,".4.1",ST77XX_GREEN);}}
void callbackAP(WiFiManager*){mostrarPortalNaTela();}

bool geocodificarCidade(){if(WiFi.status()!=WL_CONNECTED)return false;String busca=cidade+(uf.length()?", "+uf:"");BearSSL::WiFiClientSecure cl;cl.setInsecure();cl.setBufferSizes(2048,512);HTTPClient http;String url="https://geocoding-api.open-meteo.com/v1/search?name="+urlEncode(busca)+"&count=1&language=pt&format=json&countryCode=BR";if(!http.begin(cl,url))return false;http.setTimeout(10000);http.useHTTP10(true);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}DynamicJsonDocument d(2048);auto er=deserializeJson(d,http.getStream());http.end();if(er)return false;JsonArray r=d["results"].as<JsonArray>();if(r.isNull()||!r.size())return false;latitude=r[0]["latitude"]|latitude;longitude=r[0]["longitude"]|longitude;const char* n=r[0]["name"]|nullptr;if(n&&strlen(n))cidade=n;coordenadasValidas=true;salvarConfig();return true;}

bool abrirPortalConfiguracao(){WiFiManager wm;wm.setAPCallback(callbackAP);wm.setConfigPortalTimeout(0);char cb[41]={0},ub[5]={0};cidade.toCharArray(cb,sizeof(cb));uf.toCharArray(ub,sizeof(ub));WiFiManagerParameter p1("cidade","Cidade",cb,40),p2("uf","UF",ub,4);wm.addParameter(&p1);wm.addParameter(&p2);mostrarPortalNaTela();if(!wm.startConfigPortal(AP_SSID,AP_PASSWORD)||WiFi.status()!=WL_CONNECTED)return false;cidade=p1.getValue();uf=p2.getValue();cidade.trim();uf.trim();uf.toUpperCase();coordenadasValidas=false;salvarConfig();return geocodificarCidade();}
bool conectarWifiSalvo(){WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.persistent(true);if(!WiFi.SSID().length())return false;cabecalho("CLIMABOX");texto(5,retrato()?32:40,"Conectando Wi-Fi...");WiFi.begin();unsigned long ini=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-ini<WIFI_CONNECT_TIMEOUT){delay(100);yield();}return WiFi.status()==WL_CONNECTED;}

String descricaoTempo(int c){if(c==0)return"Ceu limpo";if(c<=2)return"Parc. nublado";if(c==3)return"Nublado";if(c==45||c==48)return"Neblina";if(c>=51&&c<=57)return"Garoa";if(c>=61&&c<=67)return"Chuva";if(c>=80&&c<=82)return"Pancadas";if(c>=95)return"Trovoadas";return"Variavel";}
String descricaoAQI(float v){if(isnan(v))return"Sem dados";if(v<=50)return"Boa";if(v<=100)return"Moderada";if(v<=150)return"Ruim sens.";if(v<=200)return"Ruim";if(v<=300)return"Muito ruim";return"Perigosa";}
uint16_t corAQI(float v){if(isnan(v))return ST77XX_WHITE;if(v<=50)return ST77XX_GREEN;if(v<=100)return ST77XX_YELLOW;if(v<=150)return tft.color565(255,140,0);return ST77XX_RED;}
String nivelFumaca(){if(isnan(pm25)&&isnan(aerosol))return"Sem dados";if((!isnan(pm25)&&pm25>=55)||(!isnan(aerosol)&&aerosol>=1))return"ALTA";if((!isnan(pm25)&&pm25>=35)||(!isnan(aerosol)&&aerosol>=0.5))return"MODERADA";return"BAIXA";}

void desenharIconeClima(int code,bool dia,int cx,int cy){uint16_t br=tft.color565(200,205,210);if(code<=1){if(dia){tft.fillCircle(cx,cy,9,ST77XX_YELLOW);for(int i=0;i<8;i++){float a=i*PI/4;tft.drawLine(cx+cos(a)*12,cy+sin(a)*12,cx+cos(a)*16,cy+sin(a)*16,ST77XX_YELLOW);}}else{tft.fillCircle(cx,cy,11,ST77XX_WHITE);tft.fillCircle(cx+5,cy-4,10,ST77XX_BLACK);}}else{tft.fillCircle(cx-8,cy+2,7,br);tft.fillCircle(cx+1,cy-4,10,br);tft.fillCircle(cx+11,cy+3,7,br);tft.fillRect(cx-14,cy+2,32,9,br);}}

void mostrarClima(){
  telaLimpa();String nome=cidade+(uf.length()?" - "+uf:"");
  if(retrato()){
    if(nome.length()>18)nome=nome.substring(0,18);texto(5,5,nome,ST77XX_CYAN);tft.drawFastHLine(0,18,W(),tft.color565(70,70,70));
    desenharIconeClima(weatherCode,isDay,24,45);texto(50,28,isnan(temperatura)?"--C":String(temperatura,0)+"C",ST77XX_YELLOW,3);
    texto(5,68,descricaoTempo(weatherCode));valorLinha(84,"Sensacao: ",sensacao,"C");valorLinha(99,"Umidade:  ",umidade,"%");valorLinha(114,"Vento:    ",vento," km/h");valorLinha(129,"Rajadas:  ",rajada," km/h");rodape(horaISO(weatherTime));
  }else{
    if(nome.length()>20)nome=nome.substring(0,20);texto(5,5,nome,ST77XX_CYAN);texto(126,5,"WiFi",ST77XX_GREEN);tft.drawFastHLine(0,17,W(),tft.color565(70,70,70));
    desenharIconeClima(weatherCode,isDay,30,52);texto(67,31,isnan(temperatura)?"--C":String(temperatura,0)+"C",ST77XX_YELLOW,3);texto(67,65,descricaoTempo(weatherCode));tft.drawFastHLine(0,87,W(),tft.color565(70,70,70));
    valorLinha(96,"Sens: ",sensacao,"C");valorLinha(108,"Umid: ",umidade,"%");tft.setCursor(92,96);tft.print("Vento:");tft.setCursor(92,108);tft.print(isnan(vento)?"--":String(vento,0)+" km/h");rodape(horaISO(weatherTime));
  }
}

void mostrarAtmosfera(){cabecalho("ATMOSFERA",ST77XX_CYAN);int y=retrato()?30:35,dy=retrato()?19:15;valorLinha(y,"Umidade      ",umidade,"%");valorLinha(y+=dy,"Pressao      ",pressao," hPa");valorLinha(y+=dy,"Nuvens       ",nuvens,"%");valorLinha(y+=dy,"Visibilidade ",isnan(visibilidade)?NAN:visibilidade/1000.0f," km",ST77XX_WHITE,1);valorLinha(y+=dy,"Precipitacao ",precipitacao," mm",ST77XX_WHITE,1);valorLinha(y+=dy,"Rajadas      ",rajada," km/h");rodape(horaISO(weatherTime));}
void mostrarQualidadeAr(){cabecalho("QUALIDADE AR",corAQI(aqi));int y=retrato()?30:34;texto(5,y,"AQI "+(isnan(aqi)?String("--"):String(aqi,0)),corAQI(aqi),2);texto(retrato()?5:86,y+(retrato()?23:4),descricaoAQI(aqi),corAQI(aqi));int yy=retrato()?78:62,dy=retrato()?18:15;valorLinha(yy,"PM2.5  ",pm25," ug/m3",ST77XX_WHITE,1);valorLinha(yy+=dy,"PM10   ",pm10," ug/m3",ST77XX_WHITE,1);valorLinha(yy+=dy,"Ozonio ",ozonio," ug/m3",ST77XX_WHITE,1);valorLinha(yy+=dy,"UV     ",uv,"",ST77XX_WHITE,1);rodape(horaISO(airTime));}
void mostrarPoluentes(){cabecalho("FUMACA / AR",ST77XX_YELLOW);String f=nivelFumaca();uint16_t cf=f=="ALTA"?ST77XX_RED:(f=="MODERADA"?ST77XX_YELLOW:ST77XX_GREEN);texto(5,retrato()?31:34,"Fumaca: "+f,cf);int y=retrato()?55:52,dy=retrato()?18:14;valorLinha(y,"CO      ",co," ug/m3");valorLinha(y+=dy,"NO2     ",no2," ug/m3",ST77XX_WHITE,1);valorLinha(y+=dy,"SO2     ",so2," ug/m3",ST77XX_WHITE,1);valorLinha(y+=dy,"Aerosol ",aerosol,"",ST77XX_WHITE,2);valorLinha(y+=dy,"Poeira  ",poeira," ug/m3",ST77XX_WHITE,1);rodape(horaISO(airTime));}
void mostrarTelaAtual(){switch(telaAtual){case 0:mostrarClima();break;case 1:mostrarAtmosfera();break;case 2:mostrarQualidadeAr();break;case 3:mostrarPoluentes();break;default:telaAtual=0;mostrarClima();}}
void proximaTela(){telaAtual=(telaAtual+1)%TOTAL_SCREENS;ultimaTrocaTela=millis();mostrarTelaAtual();}

bool buscarClima(){if(WiFi.status()!=WL_CONNECTED||!coordenadasValidas)return false;BearSSL::WiFiClientSecure cl;cl.setInsecure();cl.setBufferSizes(2048,512);HTTPClient http;String url="https://api.open-meteo.com/v1/forecast?latitude="+String(latitude,5)+"&longitude="+String(longitude,5)+"&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,is_day,wind_speed_10m,wind_gusts_10m,surface_pressure,precipitation,cloud_cover,visibility&timezone=auto";if(!http.begin(cl,url))return false;http.setTimeout(12000);http.useHTTP10(true);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}DynamicJsonDocument d(4096);auto er=deserializeJson(d,http.getStream());http.end();if(er)return false;JsonObject a=d["current"];temperatura=a["temperature_2m"]|NAN;umidade=a["relative_humidity_2m"]|NAN;sensacao=a["apparent_temperature"]|NAN;weatherCode=a["weather_code"]|-1;isDay=(a["is_day"]|1)==1;vento=a["wind_speed_10m"]|NAN;rajada=a["wind_gusts_10m"]|NAN;pressao=a["surface_pressure"]|NAN;precipitacao=a["precipitation"]|NAN;nuvens=a["cloud_cover"]|NAN;visibilidade=a["visibility"]|NAN;weatherTime=String((const char*)(a["time"]|""));return true;}
bool buscarAr(){if(WiFi.status()!=WL_CONNECTED||!coordenadasValidas)return false;BearSSL::WiFiClientSecure cl;cl.setInsecure();cl.setBufferSizes(2048,512);HTTPClient http;String url="https://air-quality-api.open-meteo.com/v1/air-quality?latitude="+String(latitude,5)+"&longitude="+String(longitude,5)+"&current=us_aqi,pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,aerosol_optical_depth,dust,uv_index&timezone=auto";if(!http.begin(cl,url))return false;http.setTimeout(12000);http.useHTTP10(true);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}DynamicJsonDocument d(4096);auto er=deserializeJson(d,http.getStream());http.end();if(er)return false;JsonObject a=d["current"];aqi=a["us_aqi"]|NAN;pm10=a["pm10"]|NAN;pm25=a["pm2_5"]|NAN;co=a["carbon_monoxide"]|NAN;no2=a["nitrogen_dioxide"]|NAN;so2=a["sulphur_dioxide"]|NAN;ozonio=a["ozone"]|NAN;aerosol=a["aerosol_optical_depth"]|NAN;poeira=a["dust"]|NAN;uv=a["uv_index"]|NAN;airTime=String((const char*)(a["time"]|""));return true;}
void atualizarDados(){if(WiFi.status()!=WL_CONNECTED)return;cabecalho("CLIMABOX");texto(5,retrato()?35:42,"Atualizando clima...");bool c=buscarClima();texto(5,retrato()?53:62,"Atualizando ar...");bool a=buscarAr();Serial.printf("Atualizacao clima=%s ar=%s\n",c?"OK":"ERRO",a?"OK":"ERRO");mostrarTelaAtual();ultimaTrocaTela=millis();}

void verificarBotaoConfig(){if(digitalRead(CONFIG_BUTTON)!=LOW)return;unsigned long ini=millis();while(digitalRead(CONFIG_BUTTON)==LOW){if(millis()-ini>=BUTTON_HOLD_TIME){abrirPortalConfiguracao();ultimaConsulta=0;telaAtual=0;atualizarDados();return;}delay(30);yield();}}

void setup(){
  Serial.begin(115200);
  delay(100);
  pinMode(CONFIG_BUTTON,INPUT_PULLUP);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(rotacaoAtual);
  tft.setTextWrap(false);
  telaLimpa();
  adxlOk=iniciarADXL();
  Serial.println(adxlOk?"ADXL345 OK - rotacao automatica ativa":"ADXL345 nao encontrado");
  LittleFS.begin();
  carregarConfig();
  if(!conectarWifiSalvo()){
    if(!abrirPortalConfiguracao()){
      cabecalho("SEM WIFI",ST77XX_RED);
      return;
    }
  }
  if(!coordenadasValidas&&!geocodificarCidade()){
    cabecalho("CIDADE",ST77XX_RED);
    return;
  }
  telaAtual=0;
  atualizarDados();
  ultimaConsulta=ultimaTrocaTela=millis();
}

void loop(){
  verificarBotaoConfig();
  atualizarOrientacao();
  unsigned long a=millis();
  if(WiFi.status()!=WL_CONNECTED){
    if(a-ultimaTentativaWifi>30000UL){
      ultimaTentativaWifi=a;
      WiFi.reconnect();
    }
  }
  if(a-ultimaConsulta>=WEATHER_INTERVAL){
    ultimaConsulta=a;
    if(WiFi.status()==WL_CONNECTED)atualizarDados();
  }
  if(a-ultimaTrocaTela>=SCREEN_INTERVAL)proximaTela();
  delay(20);
}

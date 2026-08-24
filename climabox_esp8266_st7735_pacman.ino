#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
extern "C" { #include <user_interface.h> }

#define TFT_CS D8
#define TFT_RST D1
#define TFT_DC D2
#define CONFIG_BUTTON 0

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

const char* AP_SSID = "ClimaBox-Setup";
const char* AP_PASSWORD = "climabox";
const char* CONFIG_FILE = "/climabox.json";

const unsigned long WIFI_CONNECT_TIMEOUT = 12000UL;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;
const unsigned long BUTTON_HOLD_TIME = 5000UL;
const unsigned long PACMAN_FRAME_INTERVAL = 33UL; // ~30 FPS

String cidade = "Manaus";
String uf = "AM";
float latitude = -3.1190f;
float longitude = -60.0217f;
bool coordenadasValidas = true;
float temperatura = NAN, sensacao = NAN, umidade = NAN, vento = NAN;
int weatherCode = -1;
bool dadosValidos = false;
unsigned long ultimaConsulta = 0, ultimoFramePacman = 0;

enum TelaModo : uint32_t { MODO_CLIMA = 0, MODO_PACMAN = 1 };
struct EstadoRtc { uint32_t magic, modo, checksum; };
const uint32_t RTC_MAGIC = 0x434C494D;
const uint32_t RTC_SLOT = 64;
TelaModo modoAtual = MODO_CLIMA;

uint32_t checksumRtc(const EstadoRtc& e) { return e.magic ^ e.modo ^ 0xA55AA55A; }
bool lerEstadoRtc(EstadoRtc& e) {
  if (!system_rtc_mem_read(RTC_SLOT, &e, sizeof(e))) return false;
  return e.magic == RTC_MAGIC && e.checksum == checksumRtc(e) && e.modo <= 1;
}
void salvarModoRtc(TelaModo modo) {
  EstadoRtc e{RTC_MAGIC, (uint32_t)modo, 0};
  e.checksum = checksumRtc(e);
  system_rtc_mem_write(RTC_SLOT, &e, sizeof(e));
}
void determinarModoInicial() {
  const rst_info* info = ESP.getResetInfoPtr();
  EstadoRtc e;
  bool ok = lerEstadoRtc(e);
  if (!info || info->reason == REASON_DEFAULT_RST || !ok) {
    modoAtual = MODO_CLIMA; salvarModoRtc(modoAtual); return;
  }
  if (info->reason == REASON_EXT_SYS_RST) {
    modoAtual = e.modo == MODO_CLIMA ? MODO_PACMAN : MODO_CLIMA;
    salvarModoRtc(modoAtual); return;
  }
  modoAtual = (TelaModo)e.modo;
}

void telaLimpa(uint16_t cor = ST77XX_BLACK) { tft.fillScreen(cor); }
void cabecalho(const String& texto, uint16_t cor = ST77XX_CYAN) {
  telaLimpa(); tft.setTextWrap(false); tft.setTextColor(cor); tft.setTextSize(2);
  tft.setCursor(5, 4); tft.print(texto);
  tft.drawFastHLine(0, 23, 160, tft.color565(60,60,60));
}
void textoLinha(int y, const String& s, uint16_t cor=ST77XX_WHITE, uint8_t tam=1) {
  tft.setTextColor(cor); tft.setTextSize(tam); tft.setCursor(5,y); tft.print(s);
}
void telaErro(const String& titulo, const String& detalhe) { cabecalho(titulo, ST77XX_RED); textoLinha(38, detalhe); }

bool salvarConfig() {
  DynamicJsonDocument doc(512);
  doc["cidade"]=cidade; doc["uf"]=uf; doc["latitude"]=latitude; doc["longitude"]=longitude; doc["coords_ok"]=coordenadasValidas;
  File f=LittleFS.open(CONFIG_FILE,"w"); if(!f) return false; serializeJson(doc,f); f.close(); return true;
}
void carregarConfig() {
  if(!LittleFS.exists(CONFIG_FILE)) return;
  File f=LittleFS.open(CONFIG_FILE,"r"); if(!f) return;
  DynamicJsonDocument doc(512); if(deserializeJson(doc,f)){f.close();return;} f.close();
  cidade=doc["cidade"]|"Manaus"; uf=doc["uf"]|"AM";
  latitude=doc["latitude"]|-3.1190f; longitude=doc["longitude"]|-60.0217f; coordenadasValidas=doc["coords_ok"]|true;
}
String urlEncode(const String& v) {
  String e; char h[4];
  for(size_t i=0;i<v.length();i++){ uint8_t c=v[i];
    if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') e+=(char)c;
    else { snprintf(h,sizeof(h),"%%%02X",c); e+=h; }
  } return e;
}

void mostrarPortalNaTela() {
  cabecalho("CONFIG WIFI",ST77XX_YELLOW);
  textoLinha(34,"Rede: "+String(AP_SSID),ST77XX_CYAN);
  textoLinha(50,"Senha: "+String(AP_PASSWORD),ST77XX_YELLOW);
  textoLinha(71,"No celular abra:"); textoLinha(87,"192.168.4.1",ST77XX_GREEN,2);
}
void callbackAP(WiFiManager* wm) { mostrarPortalNaTela(); }

bool geocodificarCidade() {
  if(WiFi.status()!=WL_CONNECTED) return false;
  cabecalho("LOCAL"); textoLinha(38,"Localizando cidade..."); textoLinha(56,cidade+" - "+uf,ST77XX_YELLOW);
  String busca=cidade; if(uf.length()) busca+=", "+uf;
  String url="https://geocoding-api.open-meteo.com/v1/search?name="+urlEncode(busca)+"&count=1&language=pt&format=json&countryCode=BR";
  BearSSL::WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  if(!http.begin(client,url)) return false; http.setTimeout(10000);
  int code=http.GET(); if(code!=HTTP_CODE_OK){http.end();return false;}
  String payload=http.getString(); http.end(); DynamicJsonDocument doc(4096);
  if(deserializeJson(doc,payload)) return false;
  JsonArray r=doc["results"].as<JsonArray>(); if(r.isNull()||!r.size()) return false;
  latitude=r[0]["latitude"].as<float>(); longitude=r[0]["longitude"].as<float>();
  const char* nome=r[0]["name"]|nullptr; if(nome&&strlen(nome)) cidade=nome;
  coordenadasValidas=true; salvarConfig(); return true;
}

bool abrirPortalConfiguracao() {
  WiFiManager wm; wm.setAPCallback(callbackAP); wm.setConfigPortalTimeout(0); wm.setMinimumSignalQuality(8); wm.setRemoveDuplicateAPs(true);
  char c[41]={0},u[5]={0}; cidade.toCharArray(c,sizeof(c)); uf.toCharArray(u,sizeof(u));
  WiFiManagerParameter titulo("<p><strong>Local do clima</strong></p>");
  WiFiManagerParameter campoCidade("cidade","Cidade",c,40); WiFiManagerParameter campoUF("uf","UF (ex.: AM)",u,4);
  wm.addParameter(&titulo); wm.addParameter(&campoCidade); wm.addParameter(&campoUF);
  mostrarPortalNaTela();
  if(!wm.startConfigPortal(AP_SSID,AP_PASSWORD)||WiFi.status()!=WL_CONNECTED) return false;
  String nc=campoCidade.getValue(), nu=campoUF.getValue(); nc.trim(); nu.trim(); nu.toUpperCase();
  if(nc.length()) cidade=nc; if(nu.length()) uf=nu; coordenadasValidas=false; salvarConfig();
  if(!geocodificarCidade()){ telaErro("CIDADE","Nao encontrada"); delay(2500); return false; }
  return true;
}

bool conectarWifiSalvo() {
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.persistent(true);
  if(!WiFi.SSID().length()) return false;
  cabecalho("CLIMABOX"); textoLinha(38,"Conectando Wi-Fi..."); textoLinha(55,WiFi.SSID(),ST77XX_YELLOW); WiFi.begin();
  unsigned long ini=millis(); int ultimo=-1;
  while(WiFi.status()!=WL_CONNECTED && millis()-ini<WIFI_CONNECT_TIMEOUT){
    int r=(WIFI_CONNECT_TIMEOUT-(millis()-ini)+999)/1000;
    if(r!=ultimo){ultimo=r;tft.fillRect(5,75,150,14,ST77XX_BLACK);textoLinha(76,"Tentativa: "+String(r)+"s");}
    delay(100); yield();
  } return WiFi.status()==WL_CONNECTED;
}

String descricaoTempo(int c) {
  if(c==0)return "Ceu limpo"; if(c==1)return "Quase limpo"; if(c==2)return "Parc. nublado"; if(c==3)return "Nublado";
  if(c==45||c==48)return "Neblina"; if(c>=51&&c<=57)return "Garoa"; if(c>=61&&c<=67)return "Chuva";
  if(c>=80&&c<=82)return "Pancadas"; if(c>=95)return "Trovoadas"; return "Tempo variavel";
}
void desenharIconeClima(int code,int cx,int cy){
  uint16_t am=ST77XX_YELLOW,br=ST77XX_WHITE,az=tft.color565(80,170,255);
  if(code==0||code==1){ tft.fillCircle(cx,cy,9,am); for(int a=0;a<8;a++){float ang=a*PI/4.0f;tft.drawLine(cx+cos(ang)*12,cy+sin(ang)*12,cx+cos(ang)*17,cy+sin(ang)*17,am);} }
  else { tft.fillCircle(cx-8,cy+2,7,br);tft.fillCircle(cx+1,cy-4,10,br);tft.fillCircle(cx+11,cy+3,7,br);tft.fillRect(cx-14,cy+2,32,9,br);
    if((code>=51&&code<=67)||(code>=80&&code<=82)||code>=95)for(int i=-8;i<=10;i+=7)tft.drawLine(cx+i,cy+15,cx+i-2,cy+21,az); }
}
void desenharTelaClima(){
  telaLimpa(); tft.setTextWrap(false); String local=cidade; if(uf.length())local+="-"+uf; if(local.length()>17)local=local.substring(0,17);
  tft.setTextColor(ST77XX_CYAN);tft.setTextSize(2);tft.setCursor(4,3);tft.print(local); desenharIconeClima(weatherCode,27,48);
  tft.setTextColor(ST77XX_YELLOW);tft.setTextSize(4);tft.setCursor(54,34); if(dadosValidos){tft.print(temperatura,0);tft.print("C");}else tft.print("--C");
  tft.setTextColor(ST77XX_WHITE);tft.setTextSize(1);tft.setCursor(5,76);tft.print(descricaoTempo(weatherCode));
  tft.setCursor(5,91);tft.print("Sens: ");if(dadosValidos)tft.print(sensacao,0);else tft.print("--");tft.print("C   Umid: ");if(dadosValidos)tft.print(umidade,0);else tft.print("--");tft.print("%");
  tft.setCursor(5,106);tft.print("Vento: ");if(dadosValidos)tft.print(vento,0);else tft.print("--");tft.print(" km/h");
  tft.setTextColor(tft.color565(100,100,100));tft.setCursor(5,119);tft.print("RST: Pac-Man");
}
bool buscarClima(){
  if(WiFi.status()!=WL_CONNECTED||!coordenadasValidas)return false;
  BearSSL::WiFiClientSecure client;client.setInsecure();HTTPClient http;
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(latitude,5)+"&longitude="+String(longitude,5)+"&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code&timezone=auto";
  if(!http.begin(client,url))return false;http.setTimeout(10000);int code=http.GET();if(code!=HTTP_CODE_OK){http.end();return false;}
  String p=http.getString();http.end();DynamicJsonDocument doc(3072);if(deserializeJson(doc,p))return false;
  temperatura=doc["current"]["temperature_2m"]|NAN;sensacao=doc["current"]["apparent_temperature"]|NAN;umidade=doc["current"]["relative_humidity_2m"]|NAN;vento=doc["current"]["wind_speed_10m"]|NAN;weatherCode=doc["current"]["weather_code"]|-1;
  dadosValidos=!isnan(temperatura);if(modoAtual==MODO_CLIMA)desenharTelaClima();return dadosValidos;
}

// ===== Pac-Man suave: buffer off-screen da pista =====
GFXcanvas16 pista(160, 58);
int animX = -90;
uint8_t fase = 0; // 0: fantasmas perseguem Pac-Man; 1: Pac-Man persegue fantasmas azuis
uint8_t boca = 0;

void ghostCanvas(GFXcanvas16& c,int x,int y,uint16_t cor,int dir){
  c.fillCircle(x,y-5,10,cor); c.fillRect(x-10,y-5,20,15,cor);
  c.fillTriangle(x-10,y+10,x-5,y+5,x,y+10,cor); c.fillTriangle(x,y+10,x+5,y+5,x+10,y+10,cor);
  c.fillCircle(x-4,y-6,4,ST77XX_WHITE);c.fillCircle(x+4,y-6,4,ST77XX_WHITE);
  uint16_t az=tft.color565(30,70,255);int dx=dir>=0?1:-1;
  c.fillCircle(x-4+dx,y-6,2,az);c.fillCircle(x+4+dx,y-6,2,az);
}
void pacCanvas(GFXcanvas16& c,int x,int y,bool aberto){
  c.fillCircle(x,y,11,ST77XX_YELLOW);
  if(aberto)c.fillTriangle(x+1,y,x+13,y-8,x+13,y+8,ST77XX_BLACK);else c.drawFastHLine(x+2,y,10,ST77XX_BLACK);
  c.fillCircle(x+2,y-5,1,ST77XX_BLACK);
}
void fundoPista(){
  pista.fillScreen(ST77XX_BLACK); uint16_t azul=tft.color565(35,70,255);
  pista.drawRect(1,1,158,56,azul);pista.drawRect(4,4,152,50,azul);
  pista.drawFastHLine(8,15,42,azul);pista.drawFastHLine(110,15,42,azul);
  pista.drawFastHLine(8,43,42,azul);pista.drawFastHLine(110,43,42,azul);
  for(int x=12;x<152;x+=14)pista.fillCircle(x,29,2,ST77XX_WHITE);
}
void prepararTelaPacman(){
  telaLimpa(); tft.setTextColor(ST77XX_YELLOW);tft.setTextSize(1);tft.setCursor(5,5);tft.print("PAC-MAN");
  tft.setTextColor(tft.color565(100,100,100));tft.setCursor(5,116);tft.print("RST: mostrar clima");
  animX=-90;fase=0;boca=0;ultimoFramePacman=millis();
}
void atualizarPacman(){
  unsigned long agora=millis(); if(agora-ultimoFramePacman<PACMAN_FRAME_INTERVAL)return; ultimoFramePacman=agora;
  fundoPista(); int y=29; boca=(boca+1)%8; bool aberta=boca<4;
  if(fase==0){
    // Pac-Man foge; tres fantasmas o perseguem.
    pacCanvas(pista,animX+105,y,aberta);
    ghostCanvas(pista,animX+72,y,tft.color565(70,220,255),1);
    ghostCanvas(pista,animX+47,y,tft.color565(255,120,210),1);
    ghostCanvas(pista,animX+22,y,tft.color565(255,70,70),1);
  } else {
    // Power pellet: fantasmas ficam azuis e Pac-Man corre atras deles.
    uint16_t medo=tft.color565(55,80,255);
    pacCanvas(pista,animX+18,y,aberta);
    ghostCanvas(pista,animX+53,y,medo,1);
    ghostCanvas(pista,animX+80,y,medo,1);
    ghostCanvas(pista,animX+107,y,medo,1);
  }
  tft.drawRGBBitmap(0,31,pista.getBuffer(),160,58);
  animX += 2; // 2 px a ~30 FPS = movimento suave
  if(animX>175){ animX=-90; fase^=1; }
}

void verificarBotaoConfig(){
  if(digitalRead(CONFIG_BUTTON)!=LOW)return;unsigned long ini=millis();
  while(digitalRead(CONFIG_BUTTON)==LOW){if(millis()-ini>=BUTTON_HOLD_TIME){cabecalho("CONFIG");textoLinha(45,"Abrindo portal...");delay(500);abrirPortalConfiguracao();ESP.restart();return;}delay(20);yield();}
}

void setup(){
  Serial.begin(115200);pinMode(CONFIG_BUTTON,INPUT_PULLUP);tft.initR(INITR_BLACKTAB);tft.setRotation(1);telaLimpa();determinarModoInicial();
  if(!LittleFS.begin()){telaErro("ERRO","LittleFS falhou");delay(1500);}carregarConfig();
  bool conectado=conectarWifiSalvo();if(!conectado){if(!abrirPortalConfiguracao()){telaErro("WIFI","Falha na configuracao");delay(2000);ESP.restart();}}
  if(!coordenadasValidas)geocodificarCidade();buscarClima();ultimaConsulta=millis();
  if(modoAtual==MODO_PACMAN)prepararTelaPacman();else desenharTelaClima();
}
void loop(){
  verificarBotaoConfig();unsigned long agora=millis();
  if(agora-ultimaConsulta>=WEATHER_INTERVAL){ultimaConsulta=agora;if(WiFi.status()==WL_CONNECTED)buscarClima();}
  if(modoAtual==MODO_PACMAN)atualizarPacman();yield();
}

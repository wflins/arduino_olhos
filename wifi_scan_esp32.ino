#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// ESP32 WROOM-32 / DevKit
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_I2C_FREQ 400000

#define SCAN_INTERVAL 15000UL
#define PAGE_INTERVAL 3000UL
#define MAX_VISIBLE_NETWORKS 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int redesEncontradas = 0;
int paginaAtual = 0;
unsigned long ultimoScan = 0;
unsigned long ultimaTrocaPagina = 0;

String limitarTexto(const String &texto, int maxChars) {
  if ((int)texto.length() <= maxChars) return texto;
  return texto.substring(0, maxChars - 1) + "~";
}

String descricaoSinal(int32_t rssi) {
  if (rssi >= -50) return "EXC";
  if (rssi >= -60) return "BOM";
  if (rssi >= -70) return "MED";
  return "FRQ";
}

void desenharIconeWifi(int x, int y, int32_t rssi) {
  int barras = 1;
  if (rssi >= -75) barras = 2;
  if (rssi >= -65) barras = 3;
  if (rssi >= -55) barras = 4;

  for (int i = 0; i < 4; i++) {
    int altura = (i + 1) * 2;
    if (i < barras) {
      display.fillRect(x + (i * 3), y + 8 - altura, 2, altura, SSD1306_WHITE);
    } else {
      display.drawRect(x + (i * 3), y + 8 - altura, 2, altura, SSD1306_WHITE);
    }
  }
}

void mostrarMensagem(const __FlashStringHelper *linha1, const __FlashStringHelper *linha2 = nullptr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.println(linha1);
  if (linha2 != nullptr) {
    display.setCursor(0, 32);
    display.println(linha2);
  }
  display.display();
}

void executarScan() {
  mostrarMensagem(F("Procurando redes..."), F("Wi-Fi 2.4 GHz"));

  WiFi.scanDelete();
  redesEncontradas = WiFi.scanNetworks(false, true);

  paginaAtual = 0;
  ultimoScan = millis();
  ultimaTrocaPagina = ultimoScan;

  Serial.print(F("Redes encontradas: "));
  Serial.println(redesEncontradas);

  if (redesEncontradas > 0) {
    for (int i = 0; i < redesEncontradas; i++) {
      Serial.print(i + 1);
      Serial.print(F(". "));
      Serial.print(WiFi.SSID(i));
      Serial.print(F("  RSSI="));
      Serial.print(WiFi.RSSI(i));
      Serial.print(F(" dBm  "));
      Serial.println(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? F("ABERTA") : F("PROTEGIDA"));
    }
  }
}

void desenharLista() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(F("WiFi: "));
  display.print(redesEncontradas);
  display.print(F(" redes"));

  if (redesEncontradas <= 0) {
    display.setCursor(0, 24);
    display.println(F("Nenhuma rede"));
    display.setCursor(0, 36);
    display.println(F("encontrada"));
    display.display();
    return;
  }

  int totalPaginas = (redesEncontradas + MAX_VISIBLE_NETWORKS - 1) / MAX_VISIBLE_NETWORKS;
  if (paginaAtual >= totalPaginas) paginaAtual = 0;

  display.setCursor(93, 0);
  display.print(paginaAtual + 1);
  display.print('/');
  display.print(totalPaginas);

  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  int inicio = paginaAtual * MAX_VISIBLE_NETWORKS;
  int fim = min(inicio + MAX_VISIBLE_NETWORKS, redesEncontradas);

  int y = 12;
  for (int i = inicio; i < fim; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "<oculta>";

    int32_t rssi = WiFi.RSSI(i);
    bool aberta = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;

    desenharIconeWifi(0, y, rssi);

    display.setCursor(14, y);
    display.print(limitarTexto(ssid, 12));

    display.setCursor(88, y);
    display.print(descricaoSinal(rssi));

    display.setCursor(113, y);
    display.print(aberta ? 'O' : '*');

    y += 10;
  }

  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS,
        true,
        false
      )) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(150);

  Serial.println(F("WiFi Scan ESP32 iniciado"));
  Serial.print(F("OLED SDA GPIO"));
  Serial.print(OLED_SDA);
  Serial.print(F(" / SCL GPIO"));
  Serial.println(OLED_SCL);

  executarScan();
  desenharLista();
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoScan >= SCAN_INTERVAL) {
    executarScan();
    desenharLista();
    return;
  }

  if (redesEncontradas > MAX_VISIBLE_NETWORKS && agora - ultimaTrocaPagina >= PAGE_INTERVAL) {
    ultimaTrocaPagina = agora;

    int totalPaginas = (redesEncontradas + MAX_VISIBLE_NETWORKS - 1) / MAX_VISIBLE_NETWORKS;
    paginaAtual = (paginaAtual + 1) % totalPaginas;
    desenharLista();
  }
}

/*
  ClimaBox - Teste de bancada
  Hardware:
    - NodeMCU ESP8266
    - OLED 0,91" I2C SSD1306 128x32
    - BMP180 (temperatura + pressao)
    - DHT11 (temperatura + umidade)

  Ligacoes:

  OLED SSD1306 0,91"
    VCC -> 3V3
    GND -> GND
    SCL -> D5 (GPIO14)
    SDA -> D6 (GPIO12)

  BMP180
    VCC -> 3V3
    GND -> GND
    SCL -> D5 (GPIO14)
    SDA -> D6 (GPIO12)

  DHT11
    VCC  -> 3V3
    GND  -> GND
    DATA -> D7 (GPIO13)

  Bibliotecas necessarias (Arduino IDE):
    - Adafruit GFX Library
    - Adafruit SSD1306
    - Adafruit BMP085 Library (tambem atende o BMP180)
    - DHT sensor library by Adafruit
    - Adafruit Unified Sensor

  Observacao:
    O OLED e o BMP180 compartilham o mesmo barramento I2C.
    Enderecos mais comuns:
      OLED   = 0x3C
      BMP180 = 0x77
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>

// -----------------------------
// Pinagem
// -----------------------------
#define I2C_SDA D6   // GPIO12
#define I2C_SCL D5   // GPIO14
#define DHT_PIN D7   // GPIO13
#define DHT_TYPE DHT11

// -----------------------------
// OLED 0,91" - 128x32
// -----------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BMP085 bmp;
DHT dht(DHT_PIN, DHT_TYPE);

bool oledOK = false;
bool bmpOK = false;
bool dhtOK = false;

unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO_LEITURA = 2500;

void scanI2C() {
  Serial.println();
  Serial.println(F("=== Scanner I2C ==="));

  byte encontrados = 0;

  for (byte endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    byte erro = Wire.endTransmission();

    if (erro == 0) {
      Serial.print(F("Dispositivo encontrado em 0x"));
      if (endereco < 16) Serial.print('0');
      Serial.print(endereco, HEX);

      if (endereco == 0x3C) Serial.print(F("  <- OLED provavel"));
      if (endereco == 0x77) Serial.print(F("  <- BMP180 provavel"));

      Serial.println();
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println(F("Nenhum dispositivo I2C encontrado."));
  } else {
    Serial.print(F("Total de dispositivos I2C: "));
    Serial.println(encontrados);
  }

  Serial.println(F("==================="));
  Serial.println();
}

void mostrarStatusInicial() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(F("ClimaBox - teste"));

  display.setCursor(0, 11);
  display.print(F("BMP180: "));
  display.println(bmpOK ? F("OK") : F("ERRO"));

  display.setCursor(0, 22);
  display.print(F("DHT11 : "));
  display.println(dhtOK ? F("OK") : F("aguardando"));

  display.display();
}

void imprimirStatusSerial(float tempDHT,
                          float umidade,
                          float tempBMP,
                          float pressaoHpa) {
  Serial.println(F("----------------------------------------"));

  Serial.print(F("DHT11 | Temperatura: "));
  if (isnan(tempDHT)) Serial.print(F("ERRO"));
  else {
    Serial.print(tempDHT, 1);
    Serial.print(F(" C"));
  }

  Serial.print(F(" | Umidade: "));
  if (isnan(umidade)) Serial.println(F("ERRO"));
  else {
    Serial.print(umidade, 0);
    Serial.println(F(" %"));
  }

  Serial.print(F("BMP180 | Temperatura: "));
  if (!bmpOK) {
    Serial.println(F("ERRO - sensor nao detectado"));
  } else {
    Serial.print(tempBMP, 1);
    Serial.print(F(" C | Pressao: "));
    Serial.print(pressaoHpa, 1);
    Serial.println(F(" hPa"));
  }

  if (!isnan(tempDHT) && bmpOK) {
    Serial.print(F("Diferenca de temperatura DHT11 x BMP180: "));
    Serial.print(tempDHT - tempBMP, 1);
    Serial.println(F(" C"));
  }

  Serial.println(F("----------------------------------------"));
  Serial.println();
}

void mostrarLeiturasOLED(float tempDHT,
                         float umidade,
                         float tempBMP,
                         float pressaoHpa) {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(F("DHT "));
  if (isnan(tempDHT)) {
    display.print(F("--.-C "));
  } else {
    display.print(tempDHT, 1);
    display.print(F("C "));
  }

  if (isnan(umidade)) {
    display.print(F("--%"));
  } else {
    display.print(umidade, 0);
    display.print(F("%"));
  }

  display.setCursor(0, 11);
  display.print(F("BMP "));
  if (!bmpOK) {
    display.print(F("ERRO"));
  } else {
    display.print(tempBMP, 1);
    display.print(F("C"));
  }

  display.setCursor(0, 22);
  display.print(F("P   "));
  if (!bmpOK) {
    display.print(F("----.- hPa"));
  } else {
    display.print(pressaoHpa, 1);
    display.print(F(" hPa"));
  }

  display.display();
}

void lerSensores() {
  float umidade = dht.readHumidity();
  float tempDHT = dht.readTemperature();

  dhtOK = !isnan(umidade) && !isnan(tempDHT);

  float tempBMP = NAN;
  float pressaoHpa = NAN;

  if (bmpOK) {
    tempBMP = bmp.readTemperature();
    pressaoHpa = bmp.readPressure() / 100.0F;
  }

  imprimirStatusSerial(tempDHT, umidade, tempBMP, pressaoHpa);
  mostrarLeiturasOLED(tempDHT, umidade, tempBMP, pressaoHpa);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F(" ClimaBox - teste de sensores ESP8266"));
  Serial.println(F(" OLED 0,91 + BMP180 + DHT11"));
  Serial.println(F("========================================"));

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  scanI2C();

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  Serial.print(F("OLED SSD1306: "));
  Serial.println(oledOK ? F("OK") : F("ERRO / nao encontrado"));

  if (oledOK) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 4);
    display.println(F("ClimaBox"));
    display.setCursor(0, 18);
    display.println(F("Testando sensores..."));
    display.display();
  }

  bmpOK = bmp.begin();
  Serial.print(F("BMP180: "));
  Serial.println(bmpOK ? F("OK") : F("ERRO / nao encontrado"));

  dht.begin();
  delay(2200);

  float testeUmidade = dht.readHumidity();
  float testeTemperatura = dht.readTemperature();
  dhtOK = !isnan(testeUmidade) && !isnan(testeTemperatura);

  Serial.print(F("DHT11: "));
  Serial.println(dhtOK ? F("OK") : F("ERRO na primeira leitura"));

  mostrarStatusInicial();
  delay(2200);

  lerSensores();
  ultimaLeitura = millis();
}

void loop() {
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = millis();
    lerSensores();
  }

  yield();
}

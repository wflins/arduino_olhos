/*
  ClimaBox - Teste sem display
  Hardware:
    - NodeMCU ESP8266
    - BMP180 (temperatura + pressao)
    - DHT11 (temperatura + umidade)

  Ligacoes:

  BMP180
    VCC -> 3V3
    GND -> GND
    SCL -> D1 (GPIO5)
    SDA -> D2 (GPIO4)

  DHT11
    VCC  -> 3V3
    GND  -> GND
    DATA -> D5 (GPIO14)

  Bibliotecas necessarias (Arduino IDE):
    - Adafruit BMP085 Library (compativel com BMP180)
    - DHT sensor library by Adafruit
    - Adafruit Unified Sensor

  Monitor Serial: 115200 baud
*/

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <DHT.h>

#define I2C_SDA D2   // GPIO4
#define I2C_SCL D1   // GPIO5
#define DHT_PIN D5   // GPIO14
#define DHT_TYPE DHT11

Adafruit_BMP085 bmp;
DHT dht(DHT_PIN, DHT_TYPE);

bool bmpOK = false;
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
      Serial.print(F("Dispositivo em 0x"));
      if (endereco < 16) Serial.print('0');
      Serial.print(endereco, HEX);

      if (endereco == 0x77) {
        Serial.print(F("  <- BMP180 provavel"));
      }

      Serial.println();
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println(F("Nenhum dispositivo I2C encontrado."));
  } else {
    Serial.print(F("Total I2C: "));
    Serial.println(encontrados);
  }

  Serial.println(F("==================="));
  Serial.println();
}

void lerSensores() {
  float umidade = dht.readHumidity();
  float tempDHT = dht.readTemperature();

  float tempBMP = NAN;
  float pressaoHpa = NAN;

  if (bmpOK) {
    tempBMP = bmp.readTemperature();
    pressaoHpa = bmp.readPressure() / 100.0F;
  }

  Serial.println(F("----------------------------------------"));
  Serial.println(F("ClimaBox - leitura local"));

  Serial.print(F("DHT11  | Temperatura: "));
  if (isnan(tempDHT)) {
    Serial.print(F("ERRO"));
  } else {
    Serial.print(tempDHT, 1);
    Serial.print(F(" C"));
  }

  Serial.print(F(" | Umidade: "));
  if (isnan(umidade)) {
    Serial.println(F("ERRO"));
  } else {
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
    Serial.print(F("Diferenca DHT11 - BMP180: "));
    Serial.print(tempDHT - tempBMP, 1);
    Serial.println(F(" C"));
  }

  Serial.println(F("----------------------------------------"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F(" ClimaBox - ESP8266 + BMP180 + DHT11"));
  Serial.println(F(" Versao de teste sem display"));
  Serial.println(F("========================================"));

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  scanI2C();

  bmpOK = bmp.begin();
  Serial.print(F("BMP180: "));
  Serial.println(bmpOK ? F("OK") : F("ERRO / nao encontrado"));

  dht.begin();
  delay(2200);

  float testeUmidade = dht.readHumidity();
  float testeTemperatura = dht.readTemperature();

  Serial.print(F("DHT11: "));
  Serial.println((!isnan(testeUmidade) && !isnan(testeTemperatura))
                 ? F("OK")
                 : F("ERRO na primeira leitura"));

  Serial.println();
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

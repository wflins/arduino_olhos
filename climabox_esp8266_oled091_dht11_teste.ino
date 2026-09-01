/*
  ClimaBox - NodeMCU ESP8266 + OLED 0,91" + DHT11

  Ligacoes:

  OLED SSD1306 0,91" I2C (128x32)
    VCC -> 3V3
    GND -> GND
    SCL/SCK -> D1 (GPIO5)
    SDA -> D2 (GPIO4)

  DHT11
    VCC  -> 3V3
    GND  -> GND
    DATA -> D5 (GPIO14)

  Bibliotecas necessarias:
    - Adafruit GFX Library
    - Adafruit SSD1306
    - DHT sensor library by Adafruit
    - Adafruit Unified Sensor

  Monitor Serial: 115200 baud
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define I2C_SDA D2
#define I2C_SCL D1

#define DHT_PIN D5
#define DHT_TYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);

bool oledOK = false;
unsigned long ultimaLeitura = 0;
const unsigned long INTERVALO_LEITURA = 2500;

void mostrarErroDHT() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("ClimaBox"));
  display.setCursor(0, 14);
  display.println(F("Erro no DHT11"));
  display.display();
}

void mostrarLeitura(float temperatura, float umidade) {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("ClimaBox"));

  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print(F("Temp: "));
  display.print(temperatura, 1);
  display.print(F(" C"));

  display.setCursor(0, 23);
  display.print(F("Umid: "));
  display.print(umidade, 0);
  display.print(F(" %"));

  display.display();
}

void lerDHT11() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  Serial.println(F("--------------------------------"));
  Serial.println(F("ClimaBox - leitura local"));

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println(F("DHT11: ERRO na leitura"));
    mostrarErroDHT();
  } else {
    Serial.print(F("Temperatura: "));
    Serial.print(temperatura, 1);
    Serial.println(F(" C"));

    Serial.print(F("Umidade: "));
    Serial.print(umidade, 0);
    Serial.println(F(" %"));

    mostrarLeitura(temperatura, umidade);
  }

  Serial.println(F("--------------------------------"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F(" ClimaBox - OLED + DHT11"));
  Serial.println(F("================================"));

  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (!oledOK) {
    Serial.println(F("OLED: ERRO / nao encontrado em 0x3C"));
  } else {
    Serial.println(F("OLED: OK (0x3C)"));

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 4);
    display.println(F("ClimaBox"));
    display.setCursor(0, 18);
    display.println(F("Iniciando DHT11..."));
    display.display();
  }

  dht.begin();
  delay(2200);

  lerDHT11();
  ultimaLeitura = millis();
}

void loop() {
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA) {
    ultimaLeitura = millis();
    lerDHT11();
  }

  yield();
}

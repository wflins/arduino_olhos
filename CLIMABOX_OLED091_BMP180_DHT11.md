# ClimaBox - NodeMCU ESP8266 + OLED 0,91 + BMP180 + DHT11

## Esquema de ligacoes

### NodeMCU ESP8266 -> OLED 0,91" I2C SSD1306

| OLED | NodeMCU |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | D1 (GPIO5) |
| SDA | D2 (GPIO4) |

### NodeMCU ESP8266 -> BMP180

| BMP180 | NodeMCU |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | D1 (GPIO5) |
| SDA | D2 (GPIO4) |

### NodeMCU ESP8266 -> DHT11

| DHT11 | NodeMCU |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DATA | D5 (GPIO14) |

> O OLED e o BMP180 usam I2C e compartilham os mesmos pinos D1/SCL e D2/SDA.

## Diagrama simplificado

```text
                         +----------------------+
                         |   NodeMCU ESP8266     |
                         |                      |
3V3 ---------------------+----------------------+---- OLED VCC
                         |                      +---- BMP180 VCC
                         |                      +---- DHT11 VCC
                         |
GND ---------------------+----------------------+---- OLED GND
                         |                      +---- BMP180 GND
                         |                      +---- DHT11 GND
                         |
D1 / GPIO5 / SCL --------+--------------------------- OLED SCL
                         +--------------------------- BMP180 SCL
                         |
D2 / GPIO4 / SDA --------+--------------------------- OLED SDA
                         +--------------------------- BMP180 SDA
                         |
D5 / GPIO14 -------------+--------------------------- DHT11 DATA
                         +----------------------+
```

## Enderecos I2C esperados

- OLED SSD1306: normalmente `0x3C`
- BMP180: `0x77`

O sketch de teste inclui um scanner I2C e mostra os enderecos encontrados no Monitor Serial.

## Bibliotecas necessarias

Instale no Library Manager da Arduino IDE:

- **Adafruit GFX Library**
- **Adafruit SSD1306**
- **Adafruit BMP085 Library** (compativel com BMP180)
- **DHT sensor library** by Adafruit
- **Adafruit Unified Sensor**

## Configuracao da Arduino IDE

- Placa: `NodeMCU 1.0 (ESP-12E Module)`
- Monitor Serial: `115200 baud`

## O que o teste faz

1. Inicializa o barramento I2C.
2. Executa scanner I2C.
3. Testa se o OLED foi detectado.
4. Testa se o BMP180 foi detectado.
5. Faz uma leitura inicial do DHT11.
6. Exibe no OLED:
   - temperatura do DHT11;
   - umidade do DHT11;
   - temperatura do BMP180;
   - pressao atmosferica em hPa.
7. Exibe as mesmas informacoes no Monitor Serial.
8. Mostra a diferenca entre a temperatura medida pelo DHT11 e pelo BMP180.

## Sketch

`climabox_esp8266_oled091_bmp180_dht11_teste.ino`

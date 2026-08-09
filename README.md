# Arduino Olhos

Projeto Arduino para exibir olhos de robo animados em um display OLED SSD1306 128x64 via I2C.

## Bibliotecas

Instale pelo Library Manager da Arduino IDE:

- Adafruit SSD1306
- Adafruit GFX Library
- FluxGarage RoboEyes

## Display

Configuracao atual:

- Resolucao: 128x64
- Interface: I2C
- Endereco: `0x3C`

Para Arduino Uno/Nano classico:

| SSD1306 | Arduino |
|---|---|
| VCC | 5V ou 3.3V conforme o modulo |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

## Funcionamento

O sketch inicial usa a biblioteca RoboEyes com:

- piscadas automaticas;
- movimento aleatorio dos olhos;
- curiosidade habilitada;
- formato personalizado dos olhos;
- atualizacao continua sem `delay()` para manter a animacao fluida.

Arquivo principal: `arduino_olhos.ino`.

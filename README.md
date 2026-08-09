# Arduino Olhos

Projeto para exibir olhos e pequenas animacoes em um display OLED SSD1306 128x64 via I2C.

O repositorio possui versoes para Arduino Nano/Uno e tambem sketches especificos para ESP32-CAM.

## Bibliotecas

Instale pelo Library Manager da Arduino IDE:

- Adafruit SSD1306
- Adafruit GFX Library
- FluxGarage RoboEyes

Para os sketches do ESP32 tambem e necessario instalar o pacote de placas **ESP32 by Espressif Systems** na Arduino IDE.

## Display OLED

Configuracao usada nos sketches:

- Resolucao: 128x64
- Controlador: SSD1306
- Interface: I2C
- Endereco: `0x3C`

---

# Ligacao no Arduino Nano / Uno

Nos Arduino Nano e Uno classicos o barramento I2C utiliza:

- SDA: A4
- SCL: A5

## Ligacoes

| SSD1306 | Arduino Nano / Uno |
|---|---|
| VCC | 5V ou 3.3V, conforme o modulo OLED |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

Representacao simplificada:

```text
OLED SSD1306        Arduino Nano
-------------       ------------
VCC  ------------> 5V ou 3.3V
GND  ------------> GND
SDA  ------------> A4
SCL  ------------> A5
```

### Alimentacao do OLED no Nano

Muitos modulos SSD1306 possuem regulador interno e aceitam alimentacao entre 3.3V e 5V, mas isso depende do modulo utilizado.

Se houver duvida, consulte a especificacao da placa OLED. Nunca alimente diretamente com 5V um modulo especificado somente para 3.3V.

---

# Ligacao no ESP32-CAM

A versao `pikachu_eyes_esp32.ino` foi preparada para um ESP32-CAM no formato AI-Thinker/compativel.

Os pinos usados pelo OLED sao:

```cpp
#define OLED_SDA 13
#define OLED_SCL 14
```

## Ligacoes do OLED

| SSD1306 | ESP32-CAM |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO13 |
| SCL | GPIO14 |

Representacao simplificada:

```text
OLED SSD1306        ESP32-CAM
-------------       ---------
VCC  ------------> 3.3V
GND  ------------> GND
SDA  ------------> GPIO13
SCL  ------------> GPIO14
```

### Por que alimentar o OLED com 3.3V no ESP32?

O ESP32 trabalha com logica de 3.3V e seus GPIOs nao devem receber sinais de 5V.

Alguns modulos OLED possuem resistores de pull-up de SDA e SCL ligados ao proprio VCC. Alimentar o OLED com 3.3V garante que o barramento I2C tambem permaneça em 3.3V.

Por isso, para o ESP32-CAM, a configuracao recomendada neste projeto e:

```text
OLED VCC = 3.3V
SDA/SCL = 3.3V
```

### GPIO13 e GPIO14 x cartao microSD

GPIO13 e GPIO14 ficam disponiveis para o OLED quando o slot microSD nao esta sendo utilizado pelo sketch.

Se futuramente o projeto utilizar o microSD em modo SD_MMC, esses pinos podem entrar em conflito com o cartao. Nesse caso sera necessario escolher outros GPIOs e alterar:

```cpp
#define OLED_SDA 13
#define OLED_SCL 14
```

---

# Alimentacao do ESP32-CAM

O ESP32-CAM possui picos de consumo maiores que um Arduino Nano, principalmente quando Wi-Fi, camera e flash/LED estao em uso.

## Alimentacao recomendada

Alimente a placa pelo pino **5V**:

| Fonte | ESP32-CAM |
|---|---|
| +5V regulado | 5V |
| GND | GND |

Representacao:

```text
Fonte 5V            ESP32-CAM
--------            ---------
+5V  ------------> 5V
GND  ------------> GND
```

Use uma fonte de **5V regulados capaz de fornecer pelo menos 1A**. Uma fonte de 5V / 2A tambem pode ser usada normalmente; a placa consumira apenas a corrente de que necessitar.

Evite alimentar o ESP32-CAM pelo pino `3.3V` a partir de uma fonte pequena ou do pino 3.3V de outro microcontrolador. O regulador/fonte pode nao suportar os picos de corrente e causar resets, travamentos ou falhas na camera.

## GND comum

Se o ESP32-CAM e o OLED estiverem sendo alimentados pela mesma fonte ou por fontes diferentes, os terras precisam estar interligados:

```text
Fonte GND -----+
               +---- ESP32-CAM GND
               |
               +---- OLED GND
```

Sem GND comum, a comunicacao I2C pode nao funcionar corretamente.

## Alimentacao durante a gravacao

Quando for programar o ESP32-CAM com um conversor USB-Serial/FTDI:

```text
USB-Serial          ESP32-CAM
----------          ---------
5V   ------------> 5V
GND  ------------> GND
TX   ------------> U0R / RX0
RX   ------------> U0T / TX0
GPIO0 ------------> GND   (somente durante o modo de gravacao)
```

TX e RX devem ser cruzados.

Depois de enviar o programa:

1. desconecte GPIO0 do GND;
2. reinicie a placa;
3. o ESP32-CAM iniciara normalmente.

Se o adaptador USB-Serial nao fornecer corrente suficiente, alimente o ESP32-CAM com uma fonte externa de 5V e mantenha o **GND da fonte, do USB-Serial e do ESP32-CAM em comum**.

> Importante: a alimentacao pode ser 5V no pino 5V do ESP32-CAM, mas os sinais seriais RX/TX do ESP32 trabalham em nivel logico de 3.3V. Use um conversor USB-Serial compativel com logica de 3.3V.

---

# Resumo das ligacoes

## Arduino Nano

```text
OLED VCC -> 5V/3.3V conforme o modulo
OLED GND -> GND
OLED SDA -> A4
OLED SCL -> A5
```

## ESP32-CAM

```text
OLED VCC -> 3.3V
OLED GND -> GND
OLED SDA -> GPIO13
OLED SCL -> GPIO14

Fonte +5V -> ESP32-CAM 5V
Fonte GND -> ESP32-CAM GND
```

---

## Funcionamento

Os sketches utilizam diferentes estilos de animacao para o SSD1306, incluindo:

- piscadas automaticas;
- movimento aleatorio dos olhos;
- diferentes expressoes;
- animacoes retro;
- atualizacao continua sem `delay()` para manter a animacao fluida.

O sketch inicial e `arduino_olhos.ino`.

A versao dos olhos inspirados no Pikachu para Arduino e `pikachu_eyes.ino`.

A versao especifica para ESP32-CAM e `pikachu_eyes_esp32.ino`.

# ClimaBox - ESP8266 + TFT ST7735

Projeto de estacao de clima para **NodeMCU ESP8266** com display **TFT SPI 1.8" 128x160 ST7735**.

Sketch principal: `climabox_esp8266_st7735.ino`.

## Recursos

- nao exige SSID/senha gravados no codigo;
- tenta o Wi-Fi salvo por no maximo 12 segundos;
- se nao houver Wi-Fi configurado ou a conexao falhar, abre o portal `ClimaBox-Setup`;
- mostra um QR Code na TFT para o celular entrar na rede de configuracao;
- captive portal do WiFiManager para escolher Wi-Fi e informar a senha;
- campos de **Cidade** e **UF** no mesmo portal;
- geocodificacao automatica da cidade pela Open-Meteo;
- clima atual pela Open-Meteo, sem chave de API;
- mostra temperatura, sensacao termica, umidade, vento e condicao;
- atualiza o clima a cada 10 minutos;
- segurando o botao **FLASH** por 5 segundos com o aparelho ligado, o portal e reaberto.

## Bibliotecas

Instale no Arduino IDE:

- **Adafruit GFX Library**
- **Adafruit ST7735 and ST7789 Library**
- **WiFiManager** by tzapu
- **ArduinoJson**
- **QRCode** by Richard Moore / ricmoo

Tambem instale o pacote de placas:

- **esp8266 by ESP8266 Community**

Selecione a placa:

`NodeMCU 1.0 (ESP-12E Module)`

## Ligacoes

As ligacoes usadas no prototipo sao:

| TFT ST7735 | NodeMCU ESP8266 |
|---|---|
| LED | 3V3 |
| SCK | D5 / GPIO14 |
| SDA / MOSI | D7 / GPIO13 |
| A0 / DC | D2 / GPIO4 |
| RESET | D1 / GPIO5 |
| CS | D8 / GPIO15 |
| GND | GND |
| VCC | VU / 5V USB |

> O VCC em `VU/5V` vale para o modulo TFT usado neste prototipo, que possui regulacao na propria placa. Os sinais SPI continuam em logica de 3.3 V. Para outro modulo, confira a tensao especificada pelo fabricante.

## Primeira inicializacao

1. Grave `climabox_esp8266_st7735.ino` no NodeMCU.
2. Se nao existir Wi-Fi salvo, o aparelho abre automaticamente `ClimaBox-Setup`.
3. A TFT mostra um QR Code.
4. Leia o QR Code com a camera do celular e aceite entrar na rede de configuracao.
5. Senha do AP de configuracao: `climabox` (o QR ja a contem).
6. O captive portal pode abrir automaticamente. Se nao abrir, acesse `192.168.4.1`.
7. Escolha sua rede Wi-Fi e informe a senha.
8. Informe `Cidade` e `UF`, por exemplo `Manaus` e `AM`.
9. Salve. O ESP8266 conecta ao Wi-Fi, encontra as coordenadas da cidade e passa a mostrar o clima.

## Se o Wi-Fi mudar

Com o ClimaBox ja ligado, segure o botao **FLASH** do NodeMCU por 5 segundos. Nao segure FLASH durante a energizacao, pois GPIO0 tambem participa do modo de gravacao do ESP8266.

## APIs

O projeto usa os servicos publicos da Open-Meteo:

- Geocoding API para transformar Cidade/UF em latitude e longitude;
- Forecast API para obter o clima atual.

Nao e necessario colocar chave de API no firmware para o uso normal deste projeto.

## Observacoes

- O firmware usa `LittleFS` para salvar Cidade/UF e coordenadas.
- As credenciais Wi-Fi sao salvas pelo proprio stack Wi-Fi/WiFiManager do ESP8266.
- O firmware nao fica indefinidamente na tela `Conectando Wi-Fi`: depois de 12 segundos sem conexao, abre o portal.
- A TFT esta configurada com `INITR_BLACKTAB`. Se outro modulo ST7735 exigir uma variante diferente, ajuste essa linha no `setup()`.

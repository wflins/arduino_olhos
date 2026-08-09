#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_random.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// ESP32-CAM: pinos livres da camera.
// Se usar cartao microSD em modo SD_MMC, GPIO13/14 podem ser ocupados.
#define OLED_SDA 13
#define OLED_SCL 14
#define OLED_I2C_FREQ 400000

#define FRAME_INTERVAL 35
#define OLHO_ESQ_X 40
#define OLHO_DIR_X 88
#define OLHO_Y 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum EstadoOlhos {
  ESTADO_NORMAL,
  ESTADO_FELIZ,
  ESTADO_IRRITADO,
  ESTADO_CURIOSO,
  ESTADO_SURPRESO,
  ESTADO_SONOLENTO,
  ESTADO_PISCADINHA
};

EstadoOlhos estadoAtual = ESTADO_NORMAL;

unsigned long ultimoFrame = 0;
unsigned long inicioEstado = 0;
unsigned long duracaoEstado = 0;
unsigned long ultimoEstadoNormal = 0;
unsigned long intervaloProximoEstado = 0;
unsigned long inicioPiscada = 0;
unsigned long duracaoPiscada = 0;
unsigned long proximaPiscada = 0;
unsigned long ultimaMudancaOlhar = 0;
unsigned long intervaloOlhar = 0;
unsigned long ultimoBounce = 0;

bool estadoEspecialAtivo = false;
bool piscando = false;

int olharX = 0;
int olharY = 0;
int alvoOlharX = 0;
int alvoOlharY = 0;
int bounceY = 0;
int direcaoBounce = -1;

int limitar(int valor, int minimo, int maximo) {
  if (valor < minimo) return minimo;
  if (valor > maximo) return maximo;
  return valor;
}

void moverAte(int &valor, int alvo) {
  if (valor < alvo) valor++;
  else if (valor > alvo) valor--;
}

void centralizarOlhar() {
  alvoOlharX = 0;
  alvoOlharY = 0;
}

void agendarOlharAleatorio() {
  intervaloOlhar = random(700UL, 2201UL);
  ultimaMudancaOlhar = millis();
}

void escolherNovoOlhar() {
  alvoOlharX = random(-4, 5);
  alvoOlharY = random(-2, 3);
  agendarOlharAleatorio();
}

void agendarProximaPiscada() {
  proximaPiscada = millis() + random(1800UL, 4801UL);
}

void iniciarPiscada() {
  if (piscando) return;
  piscando = true;
  inicioPiscada = millis();
  duracaoPiscada = random(110UL, 181UL);
}

void desenharOlhoAberto(
  int cx,
  int cy,
  int largura,
  int altura,
  int deslocamentoX,
  int deslocamentoY,
  bool brilhoExtra
) {
  int raio = largura / 2;

  display.fillRoundRect(
    cx - largura / 2,
    cy - altura / 2,
    largura,
    altura,
    raio,
    SSD1306_WHITE
  );

  int pupilaX = limitar(
    cx + deslocamentoX,
    cx - largura / 2 + 5,
    cx + largura / 2 - 5
  );

  int pupilaY = limitar(
    cy + deslocamentoY,
    cy - altura / 2 + 6,
    cy + altura / 2 - 6
  );

  display.fillCircle(pupilaX, pupilaY, 5, SSD1306_BLACK);
  display.fillCircle(pupilaX - 2, pupilaY - 2, 2, SSD1306_WHITE);

  if (brilhoExtra) {
    display.drawPixel(pupilaX + 2, pupilaY + 2, SSD1306_WHITE);
  }
}

void desenharOlhoFechado(int cx, int cy, int largura) {
  display.drawLine(cx - largura, cy, cx, cy + 3, SSD1306_WHITE);
  display.drawLine(cx, cy + 3, cx + largura, cy, SSD1306_WHITE);
  display.drawLine(cx - largura, cy + 1, cx, cy + 4, SSD1306_WHITE);
  display.drawLine(cx, cy + 4, cx + largura, cy + 1, SSD1306_WHITE);
}

void desenharOlhoFeliz(int cx, int cy) {
  display.drawLine(cx - 10, cy + 3, cx - 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 2, cx + 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 2, cx + 10, cy + 3, SSD1306_WHITE);
  display.drawLine(cx - 10, cy + 4, cx - 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 1, cx + 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 1, cx + 10, cy + 4, SSD1306_WHITE);
}

void desenharNormal() {
  if (piscando) {
    desenharOlhoFechado(OLHO_ESQ_X, OLHO_Y + bounceY, 9);
    desenharOlhoFechado(OLHO_DIR_X, OLHO_Y + bounceY, 9);
    return;
  }

  desenharOlhoAberto(OLHO_ESQ_X, OLHO_Y + bounceY, 20, 28, olharX, olharY, true);
  desenharOlhoAberto(OLHO_DIR_X, OLHO_Y + bounceY, 20, 28, olharX, olharY, true);
}

void desenharFeliz() {
  desenharOlhoFeliz(OLHO_ESQ_X, OLHO_Y + 1 + bounceY);
  desenharOlhoFeliz(OLHO_DIR_X, OLHO_Y + 1 + bounceY);
}

void desenharIrritado() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(OLHO_ESQ_X, y + 2, 20, 17, olharX, olharY, false);
  desenharOlhoAberto(OLHO_DIR_X, y + 2, 20, 17, olharX, olharY, false);

  display.drawLine(OLHO_ESQ_X - 12, y - 12, OLHO_ESQ_X + 7, y - 7, SSD1306_WHITE);
  display.drawLine(OLHO_ESQ_X - 12, y - 11, OLHO_ESQ_X + 7, y - 6, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 7, y - 7, OLHO_DIR_X + 12, y - 12, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 7, y - 6, OLHO_DIR_X + 12, y - 11, SSD1306_WHITE);
}

void desenharCurioso() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(OLHO_ESQ_X, y + 1, 18, 25, olharX, olharY, true);
  desenharOlhoAberto(OLHO_DIR_X, y, 23, 31, olharX, olharY, true);

  display.drawLine(OLHO_DIR_X - 10, y - 20, OLHO_DIR_X + 10, y - 23, SSD1306_WHITE);
}

void desenharSurpreso() {
  int y = OLHO_Y + bounceY;
  desenharOlhoAberto(OLHO_ESQ_X, y, 25, 34, olharX, olharY, true);
  desenharOlhoAberto(OLHO_DIR_X, y, 25, 34, olharX, olharY, true);
}

void desenharSonolento() {
  int y = OLHO_Y + 3;

  display.fillRoundRect(OLHO_ESQ_X - 10, y - 4, 20, 9, 4, SSD1306_WHITE);
  display.fillRoundRect(OLHO_DIR_X - 10, y - 4, 20, 9, 4, SSD1306_WHITE);

  display.drawLine(OLHO_ESQ_X - 11, y - 7, OLHO_ESQ_X + 11, y - 3, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 11, y - 7, OLHO_DIR_X + 11, y - 3, SSD1306_WHITE);

  display.fillCircle(OLHO_ESQ_X + olharX, y + 1, 2, SSD1306_BLACK);
  display.fillCircle(OLHO_DIR_X + olharX, y + 1, 2, SSD1306_BLACK);
}

void desenharPiscadinha() {
  int y = OLHO_Y + bounceY;
  desenharOlhoAberto(OLHO_ESQ_X, y, 20, 28, olharX, olharY, true);
  desenharOlhoFechado(OLHO_DIR_X, y + 1, 9);
}

void desenharEstadoAtual() {
  display.clearDisplay();

  switch (estadoAtual) {
    case ESTADO_NORMAL: desenharNormal(); break;
    case ESTADO_FELIZ: desenharFeliz(); break;
    case ESTADO_IRRITADO: desenharIrritado(); break;
    case ESTADO_CURIOSO: desenharCurioso(); break;
    case ESTADO_SURPRESO: desenharSurpreso(); break;
    case ESTADO_SONOLENTO: desenharSonolento(); break;
    case ESTADO_PISCADINHA: desenharPiscadinha(); break;
  }

  display.display();
}

void agendarProximoEstadoEspecial() {
  intervaloProximoEstado = random(4000UL, 9001UL);
  ultimoEstadoNormal = millis();
}

void voltarAoNormal() {
  estadoAtual = ESTADO_NORMAL;
  estadoEspecialAtivo = false;
  bounceY = 0;
  direcaoBounce = -1;

  escolherNovoOlhar();
  agendarProximaPiscada();
  agendarProximoEstadoEspecial();

  Serial.println(F("Expressao: normal"));
}

void escolherEstadoAleatorio() {
  int sorteio = random(100);

  estadoEspecialAtivo = true;
  inicioEstado = millis();
  duracaoEstado = random(1800UL, 4201UL);
  piscando = false;

  if (sorteio < 24) {
    estadoAtual = ESTADO_FELIZ;
    centralizarOlhar();
    duracaoEstado = random(2200UL, 4201UL);
    Serial.println(F("Expressao: feliz"));
  }
  else if (sorteio < 42) {
    estadoAtual = ESTADO_IRRITADO;
    alvoOlharX = random(-3, 4);
    alvoOlharY = 1;
    duracaoEstado = random(2200UL, 4001UL);
    Serial.println(F("Expressao: irritado"));
  }
  else if (sorteio < 62) {
    estadoAtual = ESTADO_CURIOSO;
    alvoOlharX = random(0, 2) == 0 ? -4 : 4;
    alvoOlharY = random(-1, 2);
    duracaoEstado = random(2600UL, 4801UL);
    Serial.println(F("Expressao: curioso"));
  }
  else if (sorteio < 77) {
    estadoAtual = ESTADO_SURPRESO;
    centralizarOlhar();
    duracaoEstado = random(1300UL, 2401UL);
    Serial.println(F("Expressao: surpreso"));
  }
  else if (sorteio < 90) {
    estadoAtual = ESTADO_SONOLENTO;
    alvoOlharX = random(-2, 3);
    alvoOlharY = 2;
    duracaoEstado = random(3200UL, 5801UL);
    Serial.println(F("Expressao: sonolento"));
  }
  else {
    estadoAtual = ESTADO_PISCADINHA;
    centralizarOlhar();
    duracaoEstado = random(900UL, 1701UL);
    Serial.println(F("Expressao: piscadinha"));
  }
}

void atualizarEstado() {
  unsigned long agora = millis();

  if (estadoEspecialAtivo) {
    if (agora - inicioEstado >= duracaoEstado) {
      voltarAoNormal();
    }
    return;
  }

  if (agora - ultimoEstadoNormal >= intervaloProximoEstado) {
    escolherEstadoAleatorio();
  }
}

void atualizarPiscada() {
  unsigned long agora = millis();

  if (estadoAtual != ESTADO_NORMAL) {
    piscando = false;
    return;
  }

  if (piscando) {
    if (agora - inicioPiscada >= duracaoPiscada) {
      piscando = false;
      agendarProximaPiscada();
    }
    return;
  }

  if ((long)(agora - proximaPiscada) >= 0) {
    iniciarPiscada();
  }
}

void atualizarOlhar() {
  unsigned long agora = millis();

  if (estadoAtual == ESTADO_CURIOSO) {
    if (agora - ultimaMudancaOlhar >= 700) {
      ultimaMudancaOlhar = agora;
      alvoOlharX = random(0, 2) == 0 ? -4 : 4;
      alvoOlharY = random(-2, 2);
    }
  }
  else if (estadoAtual == ESTADO_NORMAL) {
    if (agora - ultimaMudancaOlhar >= intervaloOlhar) {
      escolherNovoOlhar();
    }
  }

  moverAte(olharX, alvoOlharX);
  moverAte(olharY, alvoOlharY);
}

void atualizarBounce() {
  unsigned long agora = millis();
  unsigned long intervalo = 260;
  int limite = 1;

  if (estadoAtual == ESTADO_FELIZ) {
    intervalo = 130;
    limite = 2;
  }
  else if (estadoAtual == ESTADO_SURPRESO) {
    intervalo = 90;
    limite = 2;
  }
  else if (estadoAtual == ESTADO_SONOLENTO) {
    intervalo = 500;
  }

  if (agora - ultimoBounce < intervalo) return;
  ultimoBounce = agora;

  bounceY += direcaoBounce;

  if (bounceY <= -limite || bounceY >= limite) {
    direcaoBounce *= -1;
  }
}

void setup() {
  Serial.begin(115200);

  // Configura I2C nos pinos escolhidos do ESP32-CAM.
  if (!Wire.begin(OLED_SDA, OLED_SCL, OLED_I2C_FREQ)) {
    Serial.println(F("Falha ao iniciar I2C"));
    for (;;) {}
  }

  // O Wire ja foi iniciado acima; por isso periphBegin=false.
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS,
        true,
        false
      )) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  randomSeed(esp_random());

  display.clearDisplay();
  display.display();

  estadoAtual = ESTADO_NORMAL;
  escolherNovoOlhar();
  agendarProximaPiscada();
  agendarProximoEstadoEspecial();

  Serial.println(F("Pikachu eyes ESP32-CAM iniciado"));
  Serial.print(F("OLED SDA GPIO"));
  Serial.print(OLED_SDA);
  Serial.print(F(" / SCL GPIO"));
  Serial.println(OLED_SCL);
}

void loop() {
  unsigned long agora = millis();

  atualizarEstado();
  atualizarPiscada();
  atualizarOlhar();
  atualizarBounce();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    desenharEstadoAtual();
  }
}

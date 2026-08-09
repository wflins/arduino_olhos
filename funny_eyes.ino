#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define FRAME_INTERVAL 40
#define OLHO_ESQ_X 40
#define OLHO_DIR_X 88
#define OLHO_Y 32

// Cantos menores deixam os olhos mais retos/cartoon e menos ovais.
#define RAIO_CANTO_OLHO 4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum EstadoOlhos {
  ESTADO_NORMAL,
  ESTADO_FELIZ,
  ESTADO_IRRITADO,
  ESTADO_CURIOSO,
  ESTADO_SURPRESO,
  ESTADO_SONOLENTO,
  ESTADO_DORMINDO,
  ESTADO_PISCADINHA,
  ESTADO_TONTO,
  ESTADO_GLITCH,
  ESTADO_DESCONFIADO,
  ESTADO_APAIXONADO,
  ESTADO_PANICO,
  ESTADO_ROBO
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
unsigned long ultimoMovimento = 0;

bool estadoEspecialAtivo = false;
bool piscando = false;

int olharX = 0;
int olharY = 0;
int alvoOlharX = 0;
int alvoOlharY = 0;
int bounceY = 0;
int direcaoBounce = -1;
byte fase = 0;

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

void desenharOlhoAberto(int cx, int cy, int largura, int altura,
                        int dx, int dy, bool brilhoExtra) {
  display.fillRoundRect(
    cx - largura / 2,
    cy - altura / 2,
    largura,
    altura,
    RAIO_CANTO_OLHO,
    SSD1306_WHITE
  );

  int pupilaX = limitar(
    cx + dx,
    cx - largura / 2 + 5,
    cx + largura / 2 - 5
  );

  int pupilaY = limitar(
    cy + dy,
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

void desenharOlhoDormindo(int cx, int cy) {
  // Linha fechada mais longa e relaxada do que uma piscada normal.
  display.drawLine(cx - 11, cy, cx - 4, cy + 2, SSD1306_WHITE);
  display.drawFastHLine(cx - 4, cy + 2, 8, SSD1306_WHITE);
  display.drawLine(cx + 4, cy + 2, cx + 11, cy, SSD1306_WHITE);

  display.drawLine(cx - 10, cy + 1, cx - 4, cy + 3, SSD1306_WHITE);
  display.drawFastHLine(cx - 4, cy + 3, 8, SSD1306_WHITE);
  display.drawLine(cx + 4, cy + 3, cx + 10, cy + 1, SSD1306_WHITE);
}

void desenharZ(int x, int y, int tamanho) {
  display.drawFastHLine(x, y, tamanho, SSD1306_WHITE);
  display.drawLine(x + tamanho - 1, y, x, y + tamanho, SSD1306_WHITE);
  display.drawFastHLine(x, y + tamanho, tamanho, SSD1306_WHITE);
}

void desenharOlhoFeliz(int cx, int cy) {
  display.drawLine(cx - 10, cy + 3, cx - 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 2, cx + 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 2, cx + 10, cy + 3, SSD1306_WHITE);
  display.drawLine(cx - 10, cy + 4, cx - 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 1, cx + 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 1, cx + 10, cy + 4, SSD1306_WHITE);
}

void desenharCoracaoPreto(int cx, int cy) {
  display.fillCircle(cx - 2, cy - 2, 3, SSD1306_BLACK);
  display.fillCircle(cx + 2, cy - 2, 3, SSD1306_BLACK);
  display.fillTriangle(cx - 5, cy - 1, cx + 5, cy - 1, cx, cy + 6, SSD1306_BLACK);
}

void desenharXPupila(int cx, int cy) {
  display.drawLine(cx - 4, cy - 4, cx + 4, cy + 4, SSD1306_BLACK);
  display.drawLine(cx + 4, cy - 4, cx - 4, cy + 4, SSD1306_BLACK);
  display.drawLine(cx - 3, cy - 4, cx + 5, cy + 4, SSD1306_BLACK);
  display.drawLine(cx + 3, cy - 4, cx - 5, cy + 4, SSD1306_BLACK);
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

  display.fillRoundRect(OLHO_ESQ_X - 10, y - 4, 20, 9, 3, SSD1306_WHITE);
  display.fillRoundRect(OLHO_DIR_X - 10, y - 4, 20, 9, 3, SSD1306_WHITE);

  display.drawLine(OLHO_ESQ_X - 11, y - 7, OLHO_ESQ_X + 11, y - 3, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 11, y - 7, OLHO_DIR_X + 11, y - 3, SSD1306_WHITE);

  display.fillCircle(OLHO_ESQ_X + olharX, y + 1, 2, SSD1306_BLACK);
  display.fillCircle(OLHO_DIR_X + olharX, y + 1, 2, SSD1306_BLACK);
}

void desenharDormindo() {
  // Movimento lento de 1 pixel simula a respiracao durante o sono.
  int y = OLHO_Y + 4 + bounceY;

  desenharOlhoDormindo(OLHO_ESQ_X, y);
  desenharOlhoDormindo(OLHO_DIR_X, y);

  // O ronco cresce: nada -> Z -> ZZ -> ZZZ -> repete.
  byte etapaZ = fase % 4;

  if (etapaZ >= 1) {
    desenharZ(103, 27, 5);
  }

  if (etapaZ >= 2) {
    desenharZ(111, 17, 6);
  }

  if (etapaZ >= 3) {
    desenharZ(119, 6, 7);
  }
}

void desenharPiscadinha() {
  int y = OLHO_Y + bounceY;
  desenharOlhoAberto(OLHO_ESQ_X, y, 20, 28, olharX, olharY, true);
  desenharOlhoFechado(OLHO_DIR_X, y + 1, 9);
}

void desenharTonto() {
  int y = OLHO_Y + bounceY;

  display.fillRoundRect(OLHO_ESQ_X - 11, y - 14, 22, 28, 4, SSD1306_WHITE);
  display.fillRoundRect(OLHO_DIR_X - 11, y - 14, 22, 28, 4, SSD1306_WHITE);

  int desloca = (fase % 3) - 1;
  desenharXPupila(OLHO_ESQ_X + desloca, y);
  desenharXPupila(OLHO_DIR_X - desloca, y);
}

void desenharGlitch() {
  int deslocaE = (fase % 2 == 0) ? -4 : 3;
  int deslocaD = (fase % 3 == 0) ? 5 : -2;

  desenharOlhoAberto(OLHO_ESQ_X + deslocaE, OLHO_Y, 20, 26, olharX, olharY, false);
  desenharOlhoAberto(OLHO_DIR_X + deslocaD, OLHO_Y, 20, 26, olharX, olharY, false);

  if (fase % 2 == 0) {
    display.drawFastHLine(6, 20, 45, SSD1306_WHITE);
    display.drawFastHLine(70, 42, 52, SSD1306_WHITE);
  } else {
    display.drawFastHLine(15, 46, 37, SSD1306_WHITE);
    display.drawFastHLine(76, 18, 38, SSD1306_WHITE);
  }

  for (int x = 8 + (fase % 4); x < 124; x += 17) {
    display.drawFastHLine(x, 30 + (x % 5), 5, SSD1306_WHITE);
  }
}

void desenharDesconfiado() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(OLHO_ESQ_X, y + 2, 21, 14, 4, 1, false);
  desenharOlhoAberto(OLHO_DIR_X, y, 20, 24, 4, 0, true);

  display.drawLine(OLHO_ESQ_X - 11, y - 8, OLHO_ESQ_X + 10, y - 10, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 10, y - 14, OLHO_DIR_X + 11, y - 11, SSD1306_WHITE);
}

void desenharApaixonado() {
  int y = OLHO_Y + bounceY;

  display.fillRoundRect(OLHO_ESQ_X - 12, y - 15, 24, 30, 4, SSD1306_WHITE);
  display.fillRoundRect(OLHO_DIR_X - 12, y - 15, 24, 30, 4, SSD1306_WHITE);

  desenharCoracaoPreto(OLHO_ESQ_X, y);
  desenharCoracaoPreto(OLHO_DIR_X, y);

  if (fase % 2 == 0) {
    display.drawPixel(OLHO_ESQ_X - 9, y - 11, SSD1306_WHITE);
    display.drawPixel(OLHO_DIR_X - 9, y - 11, SSD1306_WHITE);
  }
}

void desenharPanico() {
  int tremor = (fase % 3) - 1;
  int y = OLHO_Y + tremor;

  display.fillRoundRect(OLHO_ESQ_X - 14, y - 18, 28, 36, 5, SSD1306_WHITE);
  display.fillRoundRect(OLHO_DIR_X - 14, y - 18, 28, 36, 5, SSD1306_WHITE);

  display.fillCircle(OLHO_ESQ_X + ((fase % 2) ? 3 : -3), y + 1, 3, SSD1306_BLACK);
  display.fillCircle(OLHO_DIR_X + ((fase % 2) ? -3 : 3), y + 1, 3, SSD1306_BLACK);

  display.drawLine(OLHO_ESQ_X - 13, y - 20, OLHO_ESQ_X + 10, y - 18, SSD1306_WHITE);
  display.drawLine(OLHO_DIR_X - 10, y - 18, OLHO_DIR_X + 13, y - 20, SSD1306_WHITE);
}

void desenharRobo() {
  int scan = (fase % 9) - 4;

  display.drawRoundRect(OLHO_ESQ_X - 14, OLHO_Y - 10, 28, 20, 3, SSD1306_WHITE);
  display.drawRoundRect(OLHO_DIR_X - 14, OLHO_Y - 10, 28, 20, 3, SSD1306_WHITE);

  display.fillRect(OLHO_ESQ_X + scan - 2, OLHO_Y - 6, 5, 12, SSD1306_WHITE);
  display.fillRect(OLHO_DIR_X + scan - 2, OLHO_Y - 6, 5, 12, SSD1306_WHITE);

  display.drawFastHLine(OLHO_ESQ_X - 11, OLHO_Y, 22, SSD1306_WHITE);
  display.drawFastHLine(OLHO_DIR_X - 11, OLHO_Y, 22, SSD1306_WHITE);
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
    case ESTADO_DORMINDO: desenharDormindo(); break;
    case ESTADO_PISCADINHA: desenharPiscadinha(); break;
    case ESTADO_TONTO: desenharTonto(); break;
    case ESTADO_GLITCH: desenharGlitch(); break;
    case ESTADO_DESCONFIADO: desenharDesconfiado(); break;
    case ESTADO_APAIXONADO: desenharApaixonado(); break;
    case ESTADO_PANICO: desenharPanico(); break;
    case ESTADO_ROBO: desenharRobo(); break;
  }

  display.display();
}

void agendarProximoEstadoEspecial() {
  intervaloProximoEstado = random(3500UL, 8001UL);
  ultimoEstadoNormal = millis();
}

void voltarAoNormal() {
  estadoAtual = ESTADO_NORMAL;
  estadoEspecialAtivo = false;
  bounceY = 0;
  direcaoBounce = -1;
  fase = 0;

  escolherNovoOlhar();
  agendarProximaPiscada();
  agendarProximoEstadoEspecial();

  Serial.println(F("Olhos: normal"));
}

void escolherEstadoAleatorio() {
  // random() exclui o limite superior. 1..13 inclui todos os estados especiais.
  byte escolha = random(1, 14);

  estadoAtual = (EstadoOlhos)escolha;
  estadoEspecialAtivo = true;
  inicioEstado = millis();
  duracaoEstado = random(1600UL, 3801UL);
  piscando = false;
  fase = 0;

  switch (estadoAtual) {
    case ESTADO_FELIZ:
      centralizarOlhar();
      duracaoEstado = random(2200UL, 4201UL);
      break;

    case ESTADO_IRRITADO:
      alvoOlharX = random(-3, 4);
      alvoOlharY = 1;
      break;

    case ESTADO_CURIOSO:
      alvoOlharX = random(0, 2) ? 4 : -4;
      alvoOlharY = random(-1, 2);
      duracaoEstado = random(2600UL, 4501UL);
      break;

    case ESTADO_SURPRESO:
      centralizarOlhar();
      duracaoEstado = random(1200UL, 2301UL);
      break;

    case ESTADO_SONOLENTO:
      alvoOlharX = random(-2, 3);
      alvoOlharY = 2;
      duracaoEstado = random(3000UL, 5201UL);
      break;

    case ESTADO_DORMINDO:
      centralizarOlhar();
      bounceY = 1;
      direcaoBounce = -1;
      duracaoEstado = random(5500UL, 9001UL);
      break;

    case ESTADO_PISCADINHA:
      centralizarOlhar();
      duracaoEstado = random(900UL, 1601UL);
      break;

    case ESTADO_TONTO:
      centralizarOlhar();
      duracaoEstado = random(1800UL, 3001UL);
      break;

    case ESTADO_GLITCH:
      duracaoEstado = random(1200UL, 2401UL);
      break;

    case ESTADO_DESCONFIADO:
      alvoOlharX = 4;
      alvoOlharY = 0;
      duracaoEstado = random(2200UL, 3801UL);
      break;

    case ESTADO_APAIXONADO:
      centralizarOlhar();
      duracaoEstado = random(2200UL, 4001UL);
      break;

    case ESTADO_PANICO:
      centralizarOlhar();
      duracaoEstado = random(1400UL, 2601UL);
      break;

    case ESTADO_ROBO:
      centralizarOlhar();
      duracaoEstado = random(2000UL, 3501UL);
      break;

    default:
      break;
  }

  Serial.print(F("Estado divertido: "));
  Serial.println((int)estadoAtual);
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

  if (estadoAtual == ESTADO_NORMAL) {
    if (agora - ultimaMudancaOlhar >= intervaloOlhar) {
      escolherNovoOlhar();
    }
  }
  else if (estadoAtual == ESTADO_CURIOSO) {
    if (agora - ultimaMudancaOlhar >= 650) {
      ultimaMudancaOlhar = agora;
      alvoOlharX = random(0, 2) ? 4 : -4;
      alvoOlharY = random(-2, 2);
    }
  }

  moverAte(olharX, alvoOlharX);
  moverAte(olharY, alvoOlharY);
}

void atualizarMovimentos() {
  unsigned long agora = millis();
  unsigned long intervalo = 220;
  int limite = 1;

  if (estadoAtual == ESTADO_FELIZ) {
    intervalo = 130;
    limite = 2;
  }
  else if (estadoAtual == ESTADO_SURPRESO || estadoAtual == ESTADO_PANICO) {
    intervalo = 85;
    limite = 2;
  }
  else if (estadoAtual == ESTADO_SONOLENTO) {
    intervalo = 500;
  }
  else if (estadoAtual == ESTADO_DORMINDO) {
    // Ritmo lento para simular respiracao durante o sono.
    intervalo = 650;
    limite = 1;
  }
  else if (estadoAtual == ESTADO_GLITCH ||
           estadoAtual == ESTADO_ROBO ||
           estadoAtual == ESTADO_TONTO) {
    intervalo = 100;
  }

  if (agora - ultimoMovimento < intervalo) return;

  ultimoMovimento = agora;
  fase++;

  if (estadoAtual != ESTADO_GLITCH &&
      estadoAtual != ESTADO_ROBO &&
      estadoAtual != ESTADO_TONTO) {

    bounceY += direcaoBounce;

    if (bounceY <= -limite || bounceY >= limite) {
      direcaoBounce *= -1;
    }
  }
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  // A0 fica livre no Nano. SDA = A4 e SCL = A5.
  randomSeed((unsigned long)analogRead(A0) ^ micros());

  display.clearDisplay();
  display.display();

  escolherNovoOlhar();
  agendarProximaPiscada();
  agendarProximoEstadoEspecial();

  Serial.println(F("Funny Eyes iniciado"));
}

void loop() {
  unsigned long agora = millis();

  atualizarEstado();
  atualizarPiscada();
  atualizarOlhar();
  atualizarMovimentos();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;
    desenharEstadoAtual();
  }
}

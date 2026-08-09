#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define FRAME_INTERVAL 35

#define OLHO_ESQ_X 40
#define OLHO_DIR_X 88
#define OLHO_Y 32

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ======================================================
// ESTADOS
// ======================================================

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

// ======================================================
// TEMPORIZADORES
// ======================================================

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

// ======================================================
// MOVIMENTO DOS OLHOS
// ======================================================

int olharX = 0;
int olharY = 0;

int alvoOlharX = 0;
int alvoOlharY = 0;

int bounceY = 0;
int direcaoBounce = -1;

// ======================================================
// FUNCOES AUXILIARES
// ======================================================

int limitar(int valor, int minimo, int maximo) {
  if (valor < minimo) return minimo;
  if (valor > maximo) return maximo;
  return valor;
}

void moverAte(int &valor, int alvo) {
  if (valor < alvo) valor++;
  else if (valor > alvo) valor--;
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

void centralizarOlhar() {
  alvoOlharX = 0;
  alvoOlharY = 0;
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

// ======================================================
// DESENHO BASE DOS OLHOS
// ======================================================

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

  int raioPupila = 5;

  display.fillCircle(
    pupilaX,
    pupilaY,
    raioPupila,
    SSD1306_BLACK
  );

  // Brilho principal.
  display.fillCircle(
    pupilaX - 2,
    pupilaY - 2,
    2,
    SSD1306_WHITE
  );

  // Segundo brilho deixa o olhar mais "fofo".
  if (brilhoExtra) {
    display.drawPixel(
      pupilaX + 2,
      pupilaY + 2,
      SSD1306_WHITE
    );
  }
}

void desenharOlhoFechado(int cx, int cy, int largura) {
  display.drawLine(
    cx - largura,
    cy,
    cx,
    cy + 3,
    SSD1306_WHITE
  );

  display.drawLine(
    cx,
    cy + 3,
    cx + largura,
    cy,
    SSD1306_WHITE
  );

  // Linha dupla para ficar mais visivel.
  display.drawLine(
    cx - largura,
    cy + 1,
    cx,
    cy + 4,
    SSD1306_WHITE
  );

  display.drawLine(
    cx,
    cy + 4,
    cx + largura,
    cy + 1,
    SSD1306_WHITE
  );
}

void desenharOlhoFeliz(int cx, int cy) {
  display.drawLine(cx - 10, cy + 3, cx - 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 2, cx + 4, cy - 2, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 2, cx + 10, cy + 3, SSD1306_WHITE);

  display.drawLine(cx - 10, cy + 4, cx - 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx - 4, cy - 1, cx + 4, cy - 1, SSD1306_WHITE);
  display.drawLine(cx + 4, cy - 1, cx + 10, cy + 4, SSD1306_WHITE);
}

// ======================================================
// EXPRESSOES
// ======================================================

void desenharNormal() {
  if (piscando) {
    desenharOlhoFechado(OLHO_ESQ_X, OLHO_Y + bounceY, 9);
    desenharOlhoFechado(OLHO_DIR_X, OLHO_Y + bounceY, 9);
    return;
  }

  desenharOlhoAberto(
    OLHO_ESQ_X,
    OLHO_Y + bounceY,
    20,
    28,
    olharX,
    olharY,
    true
  );

  desenharOlhoAberto(
    OLHO_DIR_X,
    OLHO_Y + bounceY,
    20,
    28,
    olharX,
    olharY,
    true
  );
}

void desenharFeliz() {
  desenharOlhoFeliz(
    OLHO_ESQ_X,
    OLHO_Y + 1 + bounceY
  );

  desenharOlhoFeliz(
    OLHO_DIR_X,
    OLHO_Y + 1 + bounceY
  );
}

void desenharIrritado() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(
    OLHO_ESQ_X,
    y + 2,
    20,
    17,
    olharX,
    olharY,
    false
  );

  desenharOlhoAberto(
    OLHO_DIR_X,
    y + 2,
    20,
    17,
    olharX,
    olharY,
    false
  );

  // Sobrancelhas inclinadas para o centro.
  display.drawLine(
    OLHO_ESQ_X - 12,
    y - 12,
    OLHO_ESQ_X + 7,
    y - 7,
    SSD1306_WHITE
  );

  display.drawLine(
    OLHO_ESQ_X - 12,
    y - 11,
    OLHO_ESQ_X + 7,
    y - 6,
    SSD1306_WHITE
  );

  display.drawLine(
    OLHO_DIR_X - 7,
    y - 7,
    OLHO_DIR_X + 12,
    y - 12,
    SSD1306_WHITE
  );

  display.drawLine(
    OLHO_DIR_X - 7,
    y - 6,
    OLHO_DIR_X + 12,
    y - 11,
    SSD1306_WHITE
  );
}

void desenharCurioso() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(
    OLHO_ESQ_X,
    y + 1,
    18,
    25,
    olharX,
    olharY,
    true
  );

  desenharOlhoAberto(
    OLHO_DIR_X,
    y,
    23,
    31,
    olharX,
    olharY,
    true
  );

  // Sobrancelha levantada.
  display.drawLine(
    OLHO_DIR_X - 10,
    y - 20,
    OLHO_DIR_X + 10,
    y - 23,
    SSD1306_WHITE
  );
}

void desenharSurpreso() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(
    OLHO_ESQ_X,
    y,
    25,
    34,
    olharX,
    olharY,
    true
  );

  desenharOlhoAberto(
    OLHO_DIR_X,
    y,
    25,
    34,
    olharX,
    olharY,
    true
  );
}

void desenharSonolento() {
  int y = OLHO_Y + 3;

  display.fillRoundRect(
    OLHO_ESQ_X - 10,
    y - 4,
    20,
    9,
    4,
    SSD1306_WHITE
  );

  display.fillRoundRect(
    OLHO_DIR_X - 10,
    y - 4,
    20,
    9,
    4,
    SSD1306_WHITE
  );

  // Palpebras pesadas.
  display.drawLine(
    OLHO_ESQ_X - 11,
    y - 7,
    OLHO_ESQ_X + 11,
    y - 3,
    SSD1306_WHITE
  );

  display.drawLine(
    OLHO_DIR_X - 11,
    y - 7,
    OLHO_DIR_X + 11,
    y - 3,
    SSD1306_WHITE
  );

  // Pequena pupila olhando para baixo.
  display.fillCircle(
    OLHO_ESQ_X + olharX,
    y + 1,
    2,
    SSD1306_BLACK
  );

  display.fillCircle(
    OLHO_DIR_X + olharX,
    y + 1,
    2,
    SSD1306_BLACK
  );
}

void desenharPiscadinha() {
  int y = OLHO_Y + bounceY;

  desenharOlhoAberto(
    OLHO_ESQ_X,
    y,
    20,
    28,
    olharX,
    olharY,
    true
  );

  desenharOlhoFechado(
    OLHO_DIR_X,
    y + 1,
    9
  );
}

// ======================================================
// RENDER
// ======================================================

void desenharEstadoAtual() {
  display.clearDisplay();

  switch (estadoAtual) {
    case ESTADO_NORMAL:
      desenharNormal();
      break;

    case ESTADO_FELIZ:
      desenharFeliz();
      break;

    case ESTADO_IRRITADO:
      desenharIrritado();
      break;

    case ESTADO_CURIOSO:
      desenharCurioso();
      break;

    case ESTADO_SURPRESO:
      desenharSurpreso();
      break;

    case ESTADO_SONOLENTO:
      desenharSonolento();
      break;

    case ESTADO_PISCADINHA:
      desenharPiscadinha();
      break;
  }

  display.display();
}

// ======================================================
// MAQUINA DE ESTADOS
// ======================================================

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

  // Impede a piscada normal de interferir nas expressoes.
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

// ======================================================
// PISCADA AUTOMATICA
// ======================================================

void atualizarPiscada() {
  unsigned long agora = millis();

  // Piscada automatica fica restrita ao estado normal.
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

// ======================================================
// OLHAR ALEATORIO E SUAVE
// ======================================================

void atualizarOlhar() {
  unsigned long agora = millis();

  // Curioso muda o foco com mais frequencia.
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

// ======================================================
// BOUNCE / MICRO MOVIMENTO
// ======================================================

void atualizarBounce() {
  unsigned long agora = millis();

  unsigned long intervalo = 260;

  if (estadoAtual == ESTADO_FELIZ) {
    intervalo = 130;
  }
  else if (estadoAtual == ESTADO_SURPRESO) {
    intervalo = 90;
  }
  else if (estadoAtual == ESTADO_SONOLENTO) {
    intervalo = 500;
  }

  if (agora - ultimoBounce < intervalo) {
    return;
  }

  ultimoBounce = agora;

  // Movimento pequeno para nao parecer que os olhos flutuam demais.
  bounceY += direcaoBounce;

  int limite = 1;

  if (estadoAtual == ESTADO_FELIZ) limite = 2;
  if (estadoAtual == ESTADO_SURPRESO) limite = 2;

  if (bounceY <= -limite || bounceY >= limite) {
    direcaoBounce *= -1;
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  randomSeed(
    (unsigned long)analogRead(A0)
    ^ micros()
  );

  display.clearDisplay();
  display.display();

  estadoAtual = ESTADO_NORMAL;

  escolherNovoOlhar();
  agendarProximaPiscada();
  agendarProximoEstadoEspecial();

  Serial.println(F("Pikachu eyes iniciado"));
}

// ======================================================
// LOOP
// ======================================================

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

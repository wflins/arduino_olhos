#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define MAR_Y 49
#define FRAME_INTERVAL 60

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------------------------------------------
// ATIVIDADES DO NAUFRAGO
// ------------------------------------------------------

enum AtividadeJohnny {
  ATIVIDADE_PARADO,
  ATIVIDADE_ANDANDO,
  ATIVIDADE_PESCANDO,
  ATIVIDADE_TELESCOPIO,
  ATIVIDADE_DORMINDO,
  ATIVIDADE_COCO,
  ATIVIDADE_SOS,
  ATIVIDADE_ACENANDO
};

AtividadeJohnny atividadeAtual = ATIVIDADE_PARADO;

unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;
unsigned long inicioAtividade = 0;
unsigned long duracaoAtividade = 0;

bool fasePasso = false;
bool faseOnda = false;
bool fasePalmeira = false;

int johnnyX = 54;
int direcaoJohnny = 1;

// Evento raro: barco passando no horizonte.
bool barcoAtivo = false;
int barcoX = -20;
int direcaoBarco = 1;

// ------------------------------------------------------
// MAR
// ------------------------------------------------------

void desenharMar() {
  for (int x = 0; x < SCREEN_WIDTH; x += 10) {
    int y1 = MAR_Y + (((x / 10) + (faseOnda ? 1 : 0)) % 2);
    int y2 = MAR_Y + 2 - (((x / 10) + (faseOnda ? 1 : 0)) % 2);

    display.drawLine(x, y1, x + 5, y2, SSD1306_WHITE);
    display.drawLine(x + 5, y2, x + 9, y1, SSD1306_WHITE);
  }

  display.drawFastHLine(0, 62, SCREEN_WIDTH, SSD1306_WHITE);
}

// ------------------------------------------------------
// ILHA
// ------------------------------------------------------

void desenharIlha() {
  // Pequena ilha arredondada.
  display.fillTriangle(24, 54, 74, 54, 62, 45, SSD1306_WHITE);
  display.fillTriangle(24, 54, 74, 54, 35, 47, SSD1306_WHITE);

  // Recortes escuros criam irregularidade na areia.
  display.fillTriangle(30, 54, 38, 50, 45, 54, SSD1306_BLACK);
  display.fillTriangle(57, 54, 65, 50, 71, 54, SSD1306_BLACK);
}

// ------------------------------------------------------
// PALMEIRA
// ------------------------------------------------------

void desenharPalmeira() {
  int troncoX = 72;

  // Tronco inclinado.
  display.drawLine(troncoX, 48, troncoX + 5, 25, SSD1306_WHITE);
  display.drawLine(troncoX + 2, 48, troncoX + 7, 25, SSD1306_WHITE);

  int topoX = troncoX + 6;
  int topoY = 24;

  // Folhas, alternando levemente para simular vento.
  int v = fasePalmeira ? 1 : -1;

  display.drawLine(topoX, topoY, topoX - 17, topoY - 6 + v, SSD1306_WHITE);
  display.drawLine(topoX, topoY, topoX - 14, topoY + 3 + v, SSD1306_WHITE);
  display.drawLine(topoX, topoY, topoX + 16, topoY - 5 - v, SSD1306_WHITE);
  display.drawLine(topoX, topoY, topoX + 15, topoY + 4 - v, SSD1306_WHITE);
  display.drawLine(topoX, topoY, topoX + 3, topoY - 13, SSD1306_WHITE);

  // Cocos.
  display.fillCircle(topoX - 2, topoY + 2, 2, SSD1306_WHITE);
  display.fillCircle(topoX + 3, topoY + 3, 2, SSD1306_WHITE);
}

// ------------------------------------------------------
// JOHNNY - BASE
// ------------------------------------------------------

void desenharJohnnyEmPe(int x, int y, int dir, bool passo) {
  // Cabeca.
  display.drawCircle(x, y - 19, 4, SSD1306_WHITE);

  // Nariz.
  display.drawPixel(x + dir * 4, y - 19, SSD1306_WHITE);

  // Barba / queixo.
  display.drawLine(x - 2, y - 16, x + 2, y - 15, SSD1306_WHITE);

  // Corpo.
  display.drawLine(x, y - 15, x, y - 5, SSD1306_WHITE);

  // Bracos.
  if (passo) {
    display.drawLine(x, y - 12, x + dir * 5, y - 8, SSD1306_WHITE);
    display.drawLine(x, y - 11, x - dir * 5, y - 14, SSD1306_WHITE);
  } else {
    display.drawLine(x, y - 12, x + dir * 5, y - 14, SSD1306_WHITE);
    display.drawLine(x, y - 11, x - dir * 5, y - 8, SSD1306_WHITE);
  }

  // Pernas.
  if (passo) {
    display.drawLine(x, y - 5, x + dir * 4, y, SSD1306_WHITE);
    display.drawLine(x, y - 5, x - dir * 4, y - 1, SSD1306_WHITE);
  } else {
    display.drawLine(x, y - 5, x + dir * 3, y - 1, SSD1306_WHITE);
    display.drawLine(x, y - 5, x - dir * 4, y, SSD1306_WHITE);
  }
}

// ------------------------------------------------------
// PESCANDO
// ------------------------------------------------------

void desenharJohnnyPescando() {
  int x = 39;
  int y = 49;

  // Sentado.
  display.drawCircle(x, y - 16, 4, SSD1306_WHITE);
  display.drawLine(x, y - 12, x + 2, y - 5, SSD1306_WHITE);
  display.drawLine(x + 2, y - 5, x + 8, y - 2, SSD1306_WHITE);
  display.drawLine(x + 8, y - 2, x + 12, y - 2, SSD1306_WHITE);

  // Braco e vara.
  display.drawLine(x + 1, y - 10, x - 4, y - 6, SSD1306_WHITE);
  display.drawLine(x - 4, y - 6, 12, 31, SSD1306_WHITE);

  // Linha ate a agua.
  display.drawLine(12, 31, 12, MAR_Y + 3, SSD1306_WHITE);

  // Boia.
  display.fillCircle(12, MAR_Y + 3 + (faseOnda ? 1 : 0), 1, SSD1306_WHITE);
}

// ------------------------------------------------------
// TELESCOPIO
// ------------------------------------------------------

void desenharJohnnyTelescopio() {
  int x = 47;
  int y = 51;

  desenharJohnnyEmPe(x, y, -1, false);

  // Telescopio apontando para o horizonte.
  display.drawLine(x - 2, y - 17, x - 12, y - 21, SSD1306_WHITE);
  display.drawLine(x - 12, y - 21, x - 18, y - 21, SSD1306_WHITE);
  display.drawLine(x - 18, y - 22, x - 18, y - 20, SSD1306_WHITE);
}

// ------------------------------------------------------
// DORMINDO
// ------------------------------------------------------

void desenharJohnnyDormindo() {
  int x = 45;
  int y = 48;

  // Corpo deitado.
  display.drawCircle(x - 10, y - 3, 4, SSD1306_WHITE);
  display.drawLine(x - 6, y - 2, x + 8, y - 2, SSD1306_WHITE);
  display.drawLine(x + 8, y - 2, x + 14, y + 1, SSD1306_WHITE);

  // Braco apoiado.
  display.drawLine(x - 5, y - 1, x, y + 2, SSD1306_WHITE);

  // ZZZ.
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(26, 31);
  display.print(F("zZ"));
}

// ------------------------------------------------------
// COCO
// ------------------------------------------------------

void desenharJohnnyCoco() {
  int x = 50;
  int y = 51;

  desenharJohnnyEmPe(x, y, 1, fasePasso);

  // Coco na mao, perto do rosto.
  display.fillCircle(x + 6, y - 15, 3, SSD1306_WHITE);
  display.drawLine(x + 2, y - 12, x + 5, y - 14, SSD1306_WHITE);
}

// ------------------------------------------------------
// SOS
// ------------------------------------------------------

void desenharJohnnySOS() {
  int x = 47;
  int y = 51;

  desenharJohnnyEmPe(x, y, 1, false);

  // Bracos erguidos.
  display.drawLine(x, y - 12, x - 6, y - 19, SSD1306_WHITE);
  display.drawLine(x, y - 12, x + 6, y - 19, SSD1306_WHITE);

  // SOS na areia.
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(31, 47);
  display.print(F("SOS"));
}

// ------------------------------------------------------
// ACENANDO
// ------------------------------------------------------

void desenharJohnnyAcenando() {
  int x = 48;
  int y = 51;

  desenharJohnnyEmPe(x, y, -1, fasePasso);

  // Braco bem levantado.
  if (fasePasso) {
    display.drawLine(x - 1, y - 12, x - 7, y - 21, SSD1306_WHITE);
    display.drawLine(x - 7, y - 21, x - 10, y - 18, SSD1306_WHITE);
  } else {
    display.drawLine(x - 1, y - 12, x - 5, y - 22, SSD1306_WHITE);
    display.drawLine(x - 5, y - 22, x - 9, y - 21, SSD1306_WHITE);
  }
}

// ------------------------------------------------------
// BARCO DISTANTE
// ------------------------------------------------------

void desenharBarco() {
  if (!barcoAtivo) return;

  int y = 35;

  display.fillTriangle(barcoX - 7, y, barcoX + 7, y, barcoX + 4, y + 4, SSD1306_WHITE);
  display.drawFastVLine(barcoX, y - 7, 7, SSD1306_WHITE);
  display.drawTriangle(barcoX + 1, y - 7, barcoX + 1, y - 1, barcoX + 6, y - 2, SSD1306_WHITE);
}

// ------------------------------------------------------
// ESCOLHA DE ATIVIDADE
// ------------------------------------------------------

void escolherNovaAtividade() {
  int sorteio = random(100);

  barcoAtivo = false;

  if (sorteio < 18) {
    atividadeAtual = ATIVIDADE_ANDANDO;
    duracaoAtividade = random(3500UL, 6001UL);
  }
  else if (sorteio < 36) {
    atividadeAtual = ATIVIDADE_PESCANDO;
    duracaoAtividade = random(4500UL, 7501UL);
  }
  else if (sorteio < 50) {
    atividadeAtual = ATIVIDADE_TELESCOPIO;
    duracaoAtividade = random(3000UL, 5001UL);
  }
  else if (sorteio < 63) {
    atividadeAtual = ATIVIDADE_DORMINDO;
    duracaoAtividade = random(4500UL, 7001UL);
  }
  else if (sorteio < 76) {
    atividadeAtual = ATIVIDADE_COCO;
    duracaoAtividade = random(3000UL, 5001UL);
  }
  else if (sorteio < 88) {
    atividadeAtual = ATIVIDADE_SOS;
    duracaoAtividade = random(3000UL, 5001UL);
  }
  else {
    // Evento raro: um barco aparece e Johnny acena desesperadamente.
    atividadeAtual = ATIVIDADE_ACENANDO;
    barcoAtivo = true;
    direcaoBarco = random(0, 2) == 0 ? 1 : -1;
    barcoX = direcaoBarco > 0 ? -18 : SCREEN_WIDTH + 18;
    duracaoAtividade = random(5000UL, 7501UL);
  }

  inicioAtividade = millis();
}

// ------------------------------------------------------
// ATUALIZACAO
// ------------------------------------------------------

void atualizarEstado() {
  unsigned long agora = millis();

  if (agora - ultimoPasso >= 180) {
    ultimoPasso = agora;
    fasePasso = !fasePasso;
    faseOnda = !faseOnda;

    if (random(0, 3) == 0) {
      fasePalmeira = !fasePalmeira;
    }
  }

  // Caminhada dentro da ilha.
  if (atividadeAtual == ATIVIDADE_ANDANDO) {
    johnnyX += direcaoJohnny;

    if (johnnyX >= 66) {
      direcaoJohnny = -1;
    }
    else if (johnnyX <= 35) {
      direcaoJohnny = 1;
    }
  }

  // Barco cruza o horizonte.
  if (barcoAtivo) {
    barcoX += direcaoBarco;
  }

  if (agora - inicioAtividade >= duracaoAtividade) {
    escolherNovaAtividade();
  }
}

// ------------------------------------------------------
// DESENHA CENA
// ------------------------------------------------------

void desenharCena() {
  display.clearDisplay();

  desenharMar();
  desenharIlha();
  desenharPalmeira();
  desenharBarco();

  switch (atividadeAtual) {
    case ATIVIDADE_ANDANDO:
      desenharJohnnyEmPe(johnnyX, 51, direcaoJohnny, fasePasso);
      break;

    case ATIVIDADE_PESCANDO:
      desenharJohnnyPescando();
      break;

    case ATIVIDADE_TELESCOPIO:
      desenharJohnnyTelescopio();
      break;

    case ATIVIDADE_DORMINDO:
      desenharJohnnyDormindo();
      break;

    case ATIVIDADE_COCO:
      desenharJohnnyCoco();
      break;

    case ATIVIDADE_SOS:
      desenharJohnnySOS();
      break;

    case ATIVIDADE_ACENANDO:
      desenharJohnnyAcenando();
      break;

    default:
      desenharJohnnyEmPe(52, 51, 1, fasePasso);
      break;
  }

  display.display();
}

// ------------------------------------------------------
// SETUP
// ------------------------------------------------------

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {}
  }

  randomSeed((unsigned long)analogRead(A0) ^ micros());

  display.clearDisplay();
  display.display();

  atividadeAtual = ATIVIDADE_PARADO;
  duracaoAtividade = 2500;
  inicioAtividade = millis();

  Serial.println(F("Johnny Castaway iniciado"));
}

// ------------------------------------------------------
// LOOP
// ------------------------------------------------------

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoFrame >= FRAME_INTERVAL) {
    ultimoFrame = agora;

    atualizarEstado();
    desenharCena();
  }
}

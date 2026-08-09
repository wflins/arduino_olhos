#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define Y_CHAO 54
#define VELOCIDADE 2
#define FRAME_INTERVAL 45
#define DISTANCIA_PEDRA 34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------------------------------------------
// ESTADO DA ANIMACAO
// ------------------------------------------------------

int xIndiana = -18;
int direcao = 1; // 1 = direita, -1 = esquerda

bool passo = false;
bool chicote = false;
byte fasePedra = 0;

unsigned long ultimoFrame = 0;
unsigned long ultimoPasso = 0;
unsigned long ultimoChicote = 0;


// ------------------------------------------------------
// DESENHA O CHAO
// ------------------------------------------------------

void desenharChao() {
  display.drawLine(0, Y_CHAO, SCREEN_WIDTH - 1, Y_CHAO, SSD1306_WHITE);

  // Pequenas pedras no caminho.
  for (int x = 6; x < SCREEN_WIDTH; x += 23) {
    display.drawPixel(x, Y_CHAO - 2, SSD1306_WHITE);
  }
}


// ------------------------------------------------------
// DESENHA A PEDRA GIGANTE
// ------------------------------------------------------

void desenharPedra(int x, int y, byte fase) {
  const int raio = 15;

  display.drawCircle(x, y, raio, SSD1306_WHITE);
  display.drawCircle(x, y, raio - 1, SSD1306_WHITE);

  // Marcas internas mudam de posicao para simular rotacao.
  switch (fase % 4) {
    case 0:
      display.drawLine(x - 10, y - 7, x + 9, y + 7, SSD1306_WHITE);
      display.drawLine(x - 5, y + 10, x + 6, y - 11, SSD1306_WHITE);
      break;

    case 1:
      display.drawLine(x - 11, y, x + 11, y, SSD1306_WHITE);
      display.drawLine(x, y - 11, x, y + 11, SSD1306_WHITE);
      break;

    case 2:
      display.drawLine(x - 9, y + 8, x + 9, y - 8, SSD1306_WHITE);
      display.drawLine(x - 7, y - 10, x + 6, y + 10, SSD1306_WHITE);
      break;

    default:
      display.drawLine(x - 11, y - 3, x + 10, y + 4, SSD1306_WHITE);
      display.drawLine(x - 3, y + 11, x + 4, y - 10, SSD1306_WHITE);
      break;
  }
}


// ------------------------------------------------------
// DESENHA POEIRA ATRAS DA PEDRA
// ------------------------------------------------------

void desenharPoeira(int xPedra, int dir) {
  int lado = -dir;

  display.drawPixel(xPedra + lado * 17, Y_CHAO - 2, SSD1306_WHITE);
  display.drawPixel(xPedra + lado * 20, Y_CHAO - 5, SSD1306_WHITE);
  display.drawPixel(xPedra + lado * 23, Y_CHAO - 1, SSD1306_WHITE);
  display.drawPixel(xPedra + lado * 26, Y_CHAO - 4, SSD1306_WHITE);
}


// ------------------------------------------------------
// DESENHA INDIANA JONES
// ------------------------------------------------------

void desenharIndiana(int x, int y, int dir, bool quadroPasso, bool usarChicote) {
  // Coordenadas base.
  int cabecaY = y - 27;
  int corpoY = y - 18;

  // ----------------------------------------------------
  // CHAPEU FEDORA
  // ----------------------------------------------------

  // Aba do chapeu.
  display.drawLine(x - 8, cabecaY - 5, x + 8, cabecaY - 5, SSD1306_WHITE);
  display.drawLine(x - 6, cabecaY - 4, x + 6, cabecaY - 4, SSD1306_WHITE);

  // Copa.
  display.fillRoundRect(x - 5, cabecaY - 11, 10, 7, 2, SSD1306_WHITE);

  // Faixa escura do chapeu.
  display.drawLine(x - 4, cabecaY - 5, x + 4, cabecaY - 5, SSD1306_BLACK);

  // ----------------------------------------------------
  // CABECA
  // ----------------------------------------------------

  display.drawCircle(x, cabecaY, 4, SSD1306_WHITE);

  // Nariz voltado para a direcao da corrida.
  display.drawPixel(x + dir * 4, cabecaY, SSD1306_WHITE);

  // ----------------------------------------------------
  // CORPO / JAQUETA
  // ----------------------------------------------------

  display.drawLine(x, cabecaY + 4, x, corpoY + 10, SSD1306_WHITE);
  display.drawLine(x - 4, corpoY, x + 4, corpoY, SSD1306_WHITE);
  display.drawLine(x - 3, corpoY + 1, x - 2, corpoY + 10, SSD1306_WHITE);
  display.drawLine(x + 3, corpoY + 1, x + 2, corpoY + 10, SSD1306_WHITE);

  // Bolsa lateral caracteristica de aventureiro.
  int bolsaX = x - dir * 5;
  display.drawRect(bolsaX - 2, corpoY + 4, 5, 6, SSD1306_WHITE);
  display.drawLine(x, corpoY, bolsaX, corpoY + 4, SSD1306_WHITE);

  // ----------------------------------------------------
  // BRACOS
  // ----------------------------------------------------

  if (quadroPasso) {
    display.drawLine(x, corpoY + 2, x + dir * 7, corpoY + 7, SSD1306_WHITE);
    display.drawLine(x, corpoY + 3, x - dir * 6, corpoY - 1, SSD1306_WHITE);
  }
  else {
    display.drawLine(x, corpoY + 2, x + dir * 6, corpoY - 2, SSD1306_WHITE);
    display.drawLine(x, corpoY + 3, x - dir * 7, corpoY + 8, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // PERNAS CORRENDO
  // ----------------------------------------------------

  int quadrilY = corpoY + 10;

  if (quadroPasso) {
    display.drawLine(x, quadrilY, x + dir * 7, y - 4, SSD1306_WHITE);
    display.drawLine(x + dir * 7, y - 4, x + dir * 10, y, SSD1306_WHITE);

    display.drawLine(x, quadrilY, x - dir * 5, y - 2, SSD1306_WHITE);
    display.drawLine(x - dir * 5, y - 2, x - dir * 8, y, SSD1306_WHITE);
  }
  else {
    display.drawLine(x, quadrilY, x + dir * 4, y - 2, SSD1306_WHITE);
    display.drawLine(x + dir * 4, y - 2, x + dir * 8, y, SSD1306_WHITE);

    display.drawLine(x, quadrilY, x - dir * 7, y - 5, SSD1306_WHITE);
    display.drawLine(x - dir * 7, y - 5, x - dir * 10, y, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // CHICOTE
  // ----------------------------------------------------

  if (usarChicote) {
    int maoX = x + dir * 6;
    int maoY = quadroPasso ? corpoY + 7 : corpoY - 2;

    // Curva simplificada com linhas sucessivas.
    display.drawLine(maoX, maoY, maoX + dir * 7, maoY - 5, SSD1306_WHITE);
    display.drawLine(maoX + dir * 7, maoY - 5, maoX + dir * 12, maoY - 2, SSD1306_WHITE);
    display.drawLine(maoX + dir * 12, maoY - 2, maoX + dir * 14, maoY + 4, SSD1306_WHITE);
  }
}


// ------------------------------------------------------
// REINICIA A CENA NO SENTIDO OPOSTO
// ------------------------------------------------------

void inverterCena() {
  direcao *= -1;

  if (direcao > 0) {
    xIndiana = -18;
  }
  else {
    xIndiana = SCREEN_WIDTH + 18;
  }
}


// ------------------------------------------------------
// ATUALIZA A ANIMACAO
// ------------------------------------------------------

void atualizarAnimacao() {
  unsigned long agora = millis();

  // Alterna as pernas e os bracos.
  if (agora - ultimoPasso >= 120) {
    ultimoPasso = agora;
    passo = !passo;
    fasePedra++;
  }

  // De vez em quando ele corre alguns instantes com o chicote levantado.
  if (agora - ultimoChicote >= 1700) {
    ultimoChicote = agora;
    chicote = !chicote;
  }

  if (agora - ultimoFrame < FRAME_INTERVAL) {
    return;
  }

  ultimoFrame = agora;

  // A pedra fica sempre atras do Indiana.
  int xPedra = xIndiana - (direcao * DISTANCIA_PEDRA);

  display.clearDisplay();

  desenharChao();

  desenharPoeira(xPedra, direcao);

  desenharPedra(
    xPedra,
    Y_CHAO - 15,
    fasePedra
  );

  desenharIndiana(
    xIndiana,
    Y_CHAO,
    direcao,
    passo,
    chicote
  );

  display.display();

  // Movimento da cena.
  xIndiana += direcao * VELOCIDADE;

  // Quando Indiana e a pedra saem completamente da tela,
  // a cena volta no sentido contrario.
  if (direcao > 0 && xIndiana > SCREEN_WIDTH + DISTANCIA_PEDRA + 20) {
    inverterCena();
  }
  else if (direcao < 0 && xIndiana < -(DISTANCIA_PEDRA + 20)) {
    inverterCena();
  }
}


// ------------------------------------------------------
// SETUP
// ------------------------------------------------------

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));

    for (;;) {
      // Para aqui caso o display nao inicialize.
    }
  }

  display.clearDisplay();
  display.display();

  Serial.println(F("Indiana iniciado"));
}


// ------------------------------------------------------
// LOOP
// ------------------------------------------------------

void loop() {
  atualizarAnimacao();
}

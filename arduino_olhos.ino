#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// Controle das mudancas aleatorias de humor.
unsigned long inicioHumor = 0;
unsigned long duracaoHumor = 0;
unsigned long ultimoHumor = 0;
unsigned long intervaloProximoHumor = 0;
bool humorAtivo = false;

// Agenda quanto tempo o robo ficara no humor normal antes de uma nova expressao.
void agendarProximoHumor() {
  intervaloProximoHumor = random(8000UL, 18001UL); // 8 a 18 segundos.
  ultimoHumor = millis();
}

// Retorna os olhos ao estado normal.
void voltarAoNormal() {
  roboEyes.setMood(DEFAULT);
  humorAtivo = false;
  agendarProximoHumor();

  Serial.println(F("Humor: normal"));
}

// Escolhe e inicia uma expressao temporaria.
void escolherHumorAleatorio() {
  int sorteio = random(100);

  // Cada humor permanece ativo entre 2,5 e 6 segundos.
  duracaoHumor = random(2500UL, 6001UL);
  inicioHumor = millis();
  humorAtivo = true;

  if (sorteio < 45) {
    // Feliz - aparece com mais frequencia.
    roboEyes.setMood(HAPPY);
    Serial.println(F("Humor: feliz"));

    // Ocasionalmente ri quando esta feliz.
    if (random(100) < 35) {
      roboEyes.anim_laugh();
    }
  }
  else if (sorteio < 75) {
    // Cansado.
    roboEyes.setMood(TIRED);
    Serial.println(F("Humor: cansado"));
  }
  else {
    // Bravo - menos frequente.
    roboEyes.setMood(ANGRY);
    Serial.println(F("Humor: bravo"));

    // Ocasionalmente faz uma pequena tremedeira de confusao/irritacao.
    if (random(100) < 35) {
      roboEyes.anim_confused();
    }
  }
}

// Atualiza a maquina de estados do humor sem bloquear as animacoes do display.
void atualizarHumor() {
  unsigned long agora = millis();

  if (humorAtivo) {
    if (agora - inicioHumor >= duracaoHumor) {
      voltarAoNormal();
    }
    return;
  }

  if (agora - ultimoHumor >= intervaloProximoHumor) {
    escolherHumorAleatorio();
  }
}

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 nao encontrado"));
    for (;;) {
      // Interrompe a execucao se o display nao inicializar.
    }
  }

  // Usa ruido de uma entrada analogica livre junto com micros() para variar
  // a sequencia de comportamentos a cada inicializacao.
  randomSeed((unsigned long)analogRead(A0) ^ micros());

  // Inicializa os olhos: largura, altura e FPS maximo.
  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);

  // Formato dos olhos.
  roboEyes.setWidth(36, 36);
  roboEyes.setHeight(36, 36);
  roboEyes.setBorderradius(8, 8);
  roboEyes.setSpacebetween(10);

  // Expressao inicial.
  roboEyes.setMood(DEFAULT);
  roboEyes.setCuriosity(ON);

  // Comportamentos automaticos.
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  // Primeira mudanca de humor acontece depois de um intervalo aleatorio.
  agendarProximoHumor();
}

void loop() {
  // Mantem as animacoes fluidas. Evite delay() no loop.
  roboEyes.update();

  // Controla as expressoes sem interromper o movimento dos olhos.
  atualizarHumor();
}

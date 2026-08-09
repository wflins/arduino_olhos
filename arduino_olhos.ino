#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define EYE_WIDTH 36
#define EYE_HEIGHT 36
#define EYE_RADIUS 8
#define EYE_SPACE 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// Tipos de comportamento do nosso robo.
enum EfeitoRobo {
  EFEITO_NENHUM,
  EFEITO_FELIZ,
  EFEITO_CANSADO,
  EFEITO_BRAVO,
  EFEITO_CURIOSO,
  EFEITO_SURPRESO,
  EFEITO_NERVOSO,
  EFEITO_FURIOSO,
  EFEITO_RISADA,
  EFEITO_PISCADINHA,
  EFEITO_SONOLENTO,
  EFEITO_CICLOPE,
  EFEITO_DESCONFIADO,
  EFEITO_TIMIDO,
  EFEITO_CONFUSO,
  EFEITO_ASSUSTADO,
  EFEITO_EMP0LGADO
};

EfeitoRobo efeitoAtual = EFEITO_NENHUM;

unsigned long inicioEfeito = 0;
unsigned long duracaoEfeito = 0;
unsigned long ultimoEfeito = 0;
unsigned long intervaloProximoEfeito = 0;
bool efeitoAtivo = false;

// Retorna uma direcao aleatoria entre as oito suportadas pela RoboEyes.
byte direcaoAleatoria() {
  const byte direcoes[] = {N, NE, E, SE, S, SW, W, NW};
  return direcoes[random(0, 8)];
}

// Agenda o proximo comportamento.
void agendarProximoEfeito() {
  // Mantem um periodo de comportamento normal entre os eventos.
  intervaloProximoEfeito = random(7000UL, 18001UL); // 7 a 18 segundos.
  ultimoEfeito = millis();
}

// Remove qualquer configuracao temporaria deixada por um efeito.
void limparEfeitos() {
  roboEyes.setMood(DEFAULT);
  roboEyes.setPosition(DEFAULT);

  roboEyes.setWidth(EYE_WIDTH, EYE_WIDTH);
  roboEyes.setHeight(EYE_HEIGHT, EYE_HEIGHT);
  roboEyes.setBorderradius(EYE_RADIUS, EYE_RADIUS);
  roboEyes.setSpacebetween(EYE_SPACE);

  roboEyes.setCyclops(OFF);
  roboEyes.setSweat(OFF);
  roboEyes.setHFlicker(OFF);
  roboEyes.setVFlicker(OFF);

  roboEyes.setCuriosity(ON);
  roboEyes.setIdleMode(ON, 2, 2);
}

void voltarAoNormal() {
  limparEfeitos();

  efeitoAtual = EFEITO_NENHUM;
  efeitoAtivo = false;

  agendarProximoEfeito();

  Serial.println(F("Estado: normal"));
}

// Inicia um dos comportamentos temporarios.
void iniciarEfeito(EfeitoRobo efeito) {
  limparEfeitos();

  efeitoAtual = efeito;
  efeitoAtivo = true;
  inicioEfeito = millis();

  // Duracao padrao. Alguns efeitos alteram esse valor abaixo.
  duracaoEfeito = random(2500UL, 5001UL);

  switch (efeito) {

    case EFEITO_FELIZ:
      roboEyes.setMood(HAPPY);
      duracaoEfeito = random(3000UL, 6001UL);
      Serial.println(F("Estado: feliz"));
      break;

    case EFEITO_CANSADO:
      roboEyes.setMood(TIRED);
      duracaoEfeito = random(4000UL, 7001UL);
      Serial.println(F("Estado: cansado"));
      break;

    case EFEITO_BRAVO:
      roboEyes.setMood(ANGRY);
      duracaoEfeito = random(2500UL, 5001UL);
      Serial.println(F("Estado: bravo"));
      break;

    case EFEITO_CURIOSO:
      // Desativa o idle temporariamente para manter o olhar fixo.
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(DEFAULT);
      roboEyes.setCuriosity(ON);
      roboEyes.setPosition(direcaoAleatoria());
      duracaoEfeito = random(2500UL, 5001UL);
      Serial.println(F("Estado: curioso"));
      break;

    case EFEITO_SURPRESO:
      // Olhos maiores simulam surpresa.
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setWidth(42, 42);
      roboEyes.setHeight(48, 48);
      duracaoEfeito = random(1800UL, 3201UL);
      Serial.println(F("Estado: surpreso"));
      break;

    case EFEITO_NERVOSO:
      roboEyes.setMood(TIRED);
      roboEyes.setSweat(ON);
      roboEyes.setHFlicker(ON, 1);
      duracaoEfeito = random(3000UL, 5501UL);
      Serial.println(F("Estado: nervoso"));
      break;

    case EFEITO_FURIOSO:
      roboEyes.setMood(ANGRY);
      roboEyes.setHFlicker(ON, 3);
      duracaoEfeito = random(1800UL, 3201UL);
      Serial.println(F("Estado: furioso"));
      break;

    case EFEITO_RISADA:
      roboEyes.setMood(HAPPY);
      roboEyes.anim_laugh();
      duracaoEfeito = random(2200UL, 4001UL);
      Serial.println(F("Estado: rindo"));
      break;

    case EFEITO_PISCADINHA:
      roboEyes.setMood(HAPPY);

      // Escolhe aleatoriamente qual olho pisca.
      if (random(0, 2) == 0) {
        roboEyes.blink(true, false);
      } else {
        roboEyes.blink(false, true);
      }

      duracaoEfeito = random(1200UL, 2201UL);
      Serial.println(F("Estado: piscadinha"));
      break;

    case EFEITO_SONOLENTO:
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(TIRED);
      roboEyes.setPosition(S);
      roboEyes.blink();
      duracaoEfeito = random(5000UL, 8001UL);
      Serial.println(F("Estado: sonolento"));
      break;

    case EFEITO_CICLOPE:
      // Evento propositalmente raro.
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setCyclops(ON);
      duracaoEfeito = random(1800UL, 3001UL);
      Serial.println(F("Estado raro: CICLOPE"));
      break;

    case EFEITO_DESCONFIADO:
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(ANGRY);
      roboEyes.setPosition(random(0, 2) == 0 ? W : E);
      duracaoEfeito = random(3000UL, 5501UL);
      Serial.println(F("Estado: desconfiado"));
      break;

    case EFEITO_TIMIDO:
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(HAPPY);
      roboEyes.setPosition(S);
      duracaoEfeito = random(2500UL, 4501UL);
      Serial.println(F("Estado: timido"));
      break;

    case EFEITO_CONFUSO:
      roboEyes.setMood(DEFAULT);
      roboEyes.anim_confused();
      duracaoEfeito = random(1800UL, 3201UL);
      Serial.println(F("Estado: confuso"));
      break;

    case EFEITO_ASSUSTADO:
      roboEyes.setIdleMode(OFF);
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setWidth(42, 42);
      roboEyes.setHeight(46, 46);
      roboEyes.setHFlicker(ON, 2);
      roboEyes.setSweat(ON);
      duracaoEfeito = random(1800UL, 3001UL);
      Serial.println(F("Estado: assustado"));
      break;

    case EFEITO_EMP0LGADO:
      roboEyes.setMood(HAPPY);
      roboEyes.setVFlicker(ON, 2);
      duracaoEfeito = random(2200UL, 4001UL);
      Serial.println(F("Estado: empolgado"));
      break;

    default:
      voltarAoNormal();
      break;
  }
}

// Sorteia um comportamento. Os efeitos mais estranhos sao menos frequentes.
void escolherEfeitoAleatorio() {
  int sorteio = random(100);

  if (sorteio < 13) {
    iniciarEfeito(EFEITO_FELIZ);          // 13%
  }
  else if (sorteio < 22) {
    iniciarEfeito(EFEITO_CANSADO);        // 9%
  }
  else if (sorteio < 29) {
    iniciarEfeito(EFEITO_BRAVO);          // 7%
  }
  else if (sorteio < 38) {
    iniciarEfeito(EFEITO_CURIOSO);        // 9%
  }
  else if (sorteio < 46) {
    iniciarEfeito(EFEITO_SURPRESO);       // 8%
  }
  else if (sorteio < 53) {
    iniciarEfeito(EFEITO_NERVOSO);        // 7%
  }
  else if (sorteio < 58) {
    iniciarEfeito(EFEITO_FURIOSO);        // 5%
  }
  else if (sorteio < 65) {
    iniciarEfeito(EFEITO_RISADA);         // 7%
  }
  else if (sorteio < 73) {
    iniciarEfeito(EFEITO_PISCADINHA);     // 8%
  }
  else if (sorteio < 80) {
    iniciarEfeito(EFEITO_SONOLENTO);      // 7%
  }
  else if (sorteio < 83) {
    iniciarEfeito(EFEITO_CICLOPE);        // 3%
  }
  else if (sorteio < 89) {
    iniciarEfeito(EFEITO_DESCONFIADO);    // 6%
  }
  else if (sorteio < 93) {
    iniciarEfeito(EFEITO_TIMIDO);         // 4%
  }
  else if (sorteio < 96) {
    iniciarEfeito(EFEITO_CONFUSO);        // 3%
  }
  else if (sorteio < 98) {
    iniciarEfeito(EFEITO_ASSUSTADO);      // 2%
  }
  else {
    iniciarEfeito(EFEITO_EMP0LGADO);      // 2%
  }
}

void atualizarPersonalidade() {
  unsigned long agora = millis();

  if (efeitoAtivo) {
    if (agora - inicioEfeito >= duracaoEfeito) {
      voltarAoNormal();
    }
    return;
  }

  if (agora - ultimoEfeito >= intervaloProximoEfeito) {
    escolherEfeitoAleatorio();
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

  // Formato padrao dos olhos.
  roboEyes.setWidth(EYE_WIDTH, EYE_WIDTH);
  roboEyes.setHeight(EYE_HEIGHT, EYE_HEIGHT);
  roboEyes.setBorderradius(EYE_RADIUS, EYE_RADIUS);
  roboEyes.setSpacebetween(EYE_SPACE);

  // Personalidade base.
  roboEyes.setMood(DEFAULT);
  roboEyes.setCuriosity(ON);

  // Comportamentos automaticos permanentes.
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  agendarProximoEfeito();

  Serial.println(F("RoboEyes iniciado"));
}

void loop() {
  // Mantem as animacoes do display suaves.
  roboEyes.update();

  // Atualiza a personalidade sem usar delay().
  atualizarPersonalidade();
}

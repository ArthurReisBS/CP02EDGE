#include <Arduino.h>
#include <esp_system.h>

// ============================================================
// CP02 Edge Computing - Projeto Motiva
// Firmware 1.0 - modulo de monitoramento de vegetacao
// Responsabilidade: 5 leituras, vetor, media, LED azul e
// nova sessao a cada 48 s usando millis().
// ============================================================

// ---------- Firmware ----------
const char *VERSAO_FIRMWARE = "1.0";

// ---------- LED RGB (catodo comum) ----------
// Estes pinos tambem podem ser reaproveitados pelo Firmware 2.0:
// vermelho = ALERTA, verde = NORMAL, azul = Firmware 1.0.
const uint8_t PIN_LED_R = 25;
const uint8_t PIN_LED_G = 26;
const uint8_t PIN_LED_B = 27;

// ---------- Regras do enunciado ----------
const uint8_t QUANTIDADE_LEITURAS = 5;
const unsigned long INTERVALO_LEITURAS_MS = 2000UL;
const unsigned long INTERVALO_SESSOES_MS = 48000UL;

// Leituras simuladas da altura da vegetacao, em centimetros.
int leituras[QUANTIDADE_LEITURAS];
uint8_t indiceLeitura = 0;

// Controle de tempo sem delays longos.
unsigned long inicioSessaoMs = 0;
unsigned long proximaLeituraMs = 0;
unsigned long proximaSessaoMs = 0;

bool sessaoEmAndamento = false;
uint32_t numeroSessao = 0;
uint32_t sessoesConcluidas = 0;

// ------------------------------------------------------------
// Retorna true quando o instante "alvo" ja foi atingido.
// A subtracao com uint32_t torna a comparacao segura mesmo
// quando millis() eventualmente sofre overflow.
// ------------------------------------------------------------
bool tempoAtingido(unsigned long agora, unsigned long alvo) {
  return (int32_t)(agora - alvo) >= 0;
}

void configurarLedRGB() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, LOW);
}

void definirCorLed(bool vermelho, bool verde, bool azul) {
  // LED RGB configurado como catodo comum no diagram.json:
  // HIGH acende o canal e LOW apaga.
  digitalWrite(PIN_LED_R, vermelho ? HIGH : LOW);
  digitalWrite(PIN_LED_G, verde ? HIGH : LOW);
  digitalWrite(PIN_LED_B, azul ? HIGH : LOW);
}

void indicarFirmware1() {
  definirCorLed(false, false, true); // azul
}

void imprimirCabecalho() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");
  Serial.print("Firmware em execucao: ");
  Serial.println(VERSAO_FIRMWARE);
  Serial.println("Indicacao visual: LED AZUL");
  Serial.println();
}

float calcularMedia() {
  long soma = 0;

  for (uint8_t i = 0; i < QUANTIDADE_LEITURAS; i++) {
    soma += leituras[i];
  }

  return soma / (float)QUANTIDADE_LEITURAS;
}

void finalizarSessao() {
  sessaoEmAndamento = false;
  sessoesConcluidas++;

  float media = calcularMedia();

  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");
  Serial.println("Proxima sessao: 48 s apos o INICIO desta sessao.");
  Serial.println("(Nao sao 48 s apos a quinta leitura.)");
  Serial.println("----------------------------------------");
  Serial.println();
}

void realizarLeitura() {
  // random(min, max) nao inclui o maximo, por isso 21.
  int alturaCm = random(10, 21);
  leituras[indiceLeitura] = alturaCm;

  Serial.print("Leitura ");
  Serial.print(indiceLeitura + 1);
  Serial.print(": ");
  Serial.print(alturaCm);
  Serial.println(" cm");

  indiceLeitura++;

  if (indiceLeitura >= QUANTIDADE_LEITURAS) {
    finalizarSessao();
  }
}

void iniciarSessao(unsigned long instanteInicio) {
  numeroSessao++;
  indiceLeitura = 0;
  sessaoEmAndamento = true;
  inicioSessaoMs = instanteInicio;

  // A primeira leitura acontece no inicio da sessao (t = 0 s).
  // As proximas ficam em t = 2, 4, 6 e 8 s.
  proximaLeituraMs = instanteInicio;

  Serial.print("Sessao ");
  Serial.print(numeroSessao);
  Serial.println(" iniciada");
}

void atualizarMonitoramento(unsigned long agora) {
  // Inicia a nova sessao com base no inicio da anterior, nunca
  // com base no fim das cinco leituras.
  if (tempoAtingido(agora, proximaSessaoMs)) {
    unsigned long instanteAgendado = proximaSessaoMs;
    proximaSessaoMs += INTERVALO_SESSOES_MS;
    iniciarSessao(instanteAgendado);
  }

  if (sessaoEmAndamento && tempoAtingido(agora, proximaLeituraMs)) {
    realizarLeitura();
    proximaLeituraMs += INTERVALO_LEITURAS_MS;
  }
}

uint32_t obterSessoesConcluidas() {
  // Ponto de integracao: o modulo de rede/OTA pode consultar
  // este contador para liberar a verificacao apos 3 sessoes.
  return sessoesConcluidas;
}

void setup() {
  Serial.begin(115200);

  configurarLedRGB();
  indicarFirmware1();

  // Semente de aleatoriedade nativa do ESP32.
  randomSeed(esp_random());

  imprimirCabecalho();

  unsigned long agora = millis();
  iniciarSessao(agora);
  proximaSessaoMs = agora + INTERVALO_SESSOES_MS;
}

void loop() {
  atualizarMonitoramento(millis());

  // Nao usar delay(48000): o loop fica livre para a futura
  // integracao com Wi-Fi, manifesto e OTA.
}

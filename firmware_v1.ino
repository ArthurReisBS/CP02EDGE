#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char *VERSAO_FIRMWARE = "1.0";

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* URL_MANIFESTO = "https://raw.githubusercontent.com/ArthurReisBS/CP02EDGE/main/version.json";
bool verificacaoOtaRealizada = false;

const uint8_t PIN_LED_R = 25;
const uint8_t PIN_LED_G = 26;
const uint8_t PIN_LED_B = 27;

const uint8_t QUANTIDADE_LEITURAS = 5;
const unsigned long INTERVALO_LEITURAS_MS = 2000UL;
const unsigned long INTERVALO_SESSOES_MS = 48000UL;

int leituras[QUANTIDADE_LEITURAS];
uint8_t indiceLeitura = 0;
unsigned long proximaLeituraMs = 0;
unsigned long proximaSessaoMs = 0;
bool sessaoEmAndamento = false;
uint32_t numeroSessao = 0;
uint32_t sessoesConcluidas = 0;

bool tempoAtingido(unsigned long agora, unsigned long alvo) {
  return (int32_t)(agora - alvo) >= 0;
}

void configurarLedRGB() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  // Firmware 1.0: LED Azul
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, HIGH);
}

void imprimirCabecalho() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");
  Serial.println("Estado: LED Azul (FW 1.0)");
  Serial.println();
}

void conectarWiFi() {
  Serial.print("A ligar a rede Wi-Fi ");
  Serial.print(ssid);
  WiFi.begin(ssid, password, 6);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi ligado com sucesso!");
  } else {
    Serial.println("Erro: Nao foi possivel ligar ao Wi-Fi.");
  }
}

void consultarManifestoOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro: Sem ligacao Wi-Fi para procurar o manifesto.");
    return;
  }
  Serial.println("A consultar manifesto de versao...");
  HTTPClient http;
  http.begin(URL_MANIFESTO);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError erro = deserializeJson(doc, payload);

    if (erro) {
      Serial.println("Erro: Falha ao interpretar o ficheiro version.json.");
      http.end();
      return;
    }

    const char* versaoDisponivel = doc["version"];
    const char* urlBinario = doc["url"];

    Serial.print("Versao instalada: ");
    Serial.println(VERSAO_FIRMWARE);
    Serial.print("Versao disponivel: ");
    Serial.println(versaoDisponivel);

    if (String(versaoDisponivel) != String(VERSAO_FIRMWARE)) {
      Serial.println("Atualizacao encontrada! A preparar descarregamento...");
      Serial.print("URL do Firmware: ");
      Serial.println(urlBinario);
    } else {
      Serial.println("A versao instalada ja e a mais recente.");
    }
  } else {
    Serial.print("Erro: O manifesto nao pode ser acedido. Codigo HTTP: ");
    Serial.println(httpCode);
  }
  http.end();
}

void finalizarSessao() {
  sessaoEmAndamento = false;
  sessoesConcluidas++;

  long soma = 0;
  Serial.println();
  Serial.println("--- Resultado da sessao ---");
  for (uint8_t i = 0; i < QUANTIDADE_LEITURAS; i++) {
    soma += leituras[i];
  }
  float media = soma / (float)QUANTIDADE_LEITURAS;
  
  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");
  Serial.print("Sessoes concluidas: ");
  Serial.println(sessoesConcluidas);
  Serial.println("Proxima sessao em 48 segundos.");
  Serial.println("---------------------------");
  Serial.println();
}

void realizarLeitura() {
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
  proximaLeituraMs = instanteInicio;
  Serial.print("Sessao ");
  Serial.print(numeroSessao);
  Serial.println(" iniciada");
}

void atualizarMonitoramento(unsigned long agora) {
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

void setup() {
  Serial.begin(115200);
  configurarLedRGB();
  randomSeed(esp_random());
  imprimirCabecalho();

  unsigned long agora = millis();
  iniciarSessao(agora);
  proximaSessaoMs = agora + INTERVALO_SESSOES_MS;
}

void loop() {
  atualizarMonitoramento(millis());
  
  if (sessoesConcluidas >= 3 && !verificacaoOtaRealizada) {
    verificacaoOtaRealizada = true; 
    if (WiFi.status() != WL_CONNECTED) {
      conectarWiFi();
    }
    consultarManifestoOTA();
  }
}
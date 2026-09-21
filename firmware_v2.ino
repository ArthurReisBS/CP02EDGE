#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

enum ResultadoOTA {
  OTA_OK,
  OTA_SEM_WIFI,
  OTA_DOWNLOAD_FALHOU,
  OTA_SEM_ESPACO,
  OTA_GRAVACAO_FALHOU
};

const char *VERSAO_FIRMWARE = "2.0"; 

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

const bool MODO_TESTE_FIXO = false;
const uint8_t TESTE_ATIVO = 0;
const int leiturasTeste[3][QUANTIDADE_LEITURAS] = {
  {18, 17, 16, 19, 20},
  {15, 14, 15, 16, 15},
  {12, 13, 14, 10, 14}
};

int leituras[QUANTIDADE_LEITURAS];
uint8_t indiceLeitura = 0;
unsigned long proximaLeituraMs = 0;
unsigned long proximaSessaoMs = 0;
bool sessaoEmAndamento = false;
bool estadoAlerta = false; 
uint32_t numeroSessao = 0;
uint32_t sessoesConcluidas = 0;

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
  digitalWrite(PIN_LED_R, vermelho ? HIGH : LOW);
  digitalWrite(PIN_LED_G, verde ? HIGH : LOW);
  digitalWrite(PIN_LED_B, azul ? HIGH : LOW);
}

void indicarEstado(bool alerta) {
  if (alerta) {
    definirCorLed(true, false, false);
  } else {
    definirCorLed(false, true, false); 
  }
}

void imprimirCabecalho() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  Serial.println("========================================");
  Serial.print("Firmware em execucao: ");
  Serial.println(VERSAO_FIRMWARE);
  Serial.println("Estado inicial: NORMAL (LED verde)");
  Serial.println();
}

void conectarWiFi() {
  Serial.print("Conectando a rede Wi-Fi ");
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
    Serial.println("Wi-Fi conectado com sucesso!");
  } else {
    Serial.println("Erro: Nao foi possivel conectar ao Wi-Fi.");
  }
}

void mostrarProgressoOTA(size_t escritos, size_t total) {
  static int ultimaFatia = -1;
  if (total == 0) return;
  int fatia = (escritos * 100) / total / 10;
  if (fatia != ultimaFatia) {
    ultimaFatia = fatia;
    Serial.print("Gravado: ");
    Serial.print(fatia * 10);
    Serial.println("%");
  }
}

ResultadoOTA executarOTA(const String &url) {
  if (WiFi.status() != WL_CONNECTED) return OTA_SEM_WIFI;

  WiFiClientSecure cliente;
  cliente.setInsecure();

  HTTPClient http;
  if (!http.begin(cliente, url)) return OTA_DOWNLOAD_FALHOU;

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print("Codigo HTTP do binario: ");
    Serial.println(codigo);
    http.end();
    return OTA_DOWNLOAD_FALHOU;
  }

  int tamanho = http.getSize();
  if (tamanho <= 0) {
    http.end();
    return OTA_DOWNLOAD_FALHOU;
  }
  Serial.print("Tamanho do firmware: ");
  Serial.print(tamanho);
  Serial.println(" bytes");

  if (!Update.begin(tamanho)) {
    http.end();
    return OTA_SEM_ESPACO;
  }

  Update.onProgress(mostrarProgressoOTA);
  size_t gravados = Update.writeStream(http.getStream());
  http.end();

  if (gravados != (size_t)tamanho) {
    Update.abort();
    return OTA_GRAVACAO_FALHOU;
  }
  if (!Update.end(true) || !Update.isFinished()) {
    return OTA_GRAVACAO_FALHOU;
  }
  return OTA_OK;
}

void aplicarAtualizacao(const String &url) {
  Serial.println("Baixando e gravando o novo firmware...");
  ResultadoOTA resultado = executarOTA(url);

  switch (resultado) {
    case OTA_OK:
      Serial.println("Gravacao concluida. Reiniciando na versao nova...");
      delay(500);
      ESP.restart();
      break;
    case OTA_SEM_WIFI:
      Serial.println("Erro: A conexao Wi-Fi caiu durante a atualizacao.");
      break;
    case OTA_DOWNLOAD_FALHOU:
      Serial.println("Erro: Nao foi possivel baixar o arquivo .bin.");
      break;
    case OTA_SEM_ESPACO:
      Serial.println("Erro: Espaco insuficiente na particao OTA.");
      break;
    case OTA_GRAVACAO_FALHOU:
      Serial.print("Erro: Falha ao gravar o firmware. ");
      Serial.println(Update.errorString());
      break;
  }
}

void consultarManifestoOTA() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro: Sem conexao Wi-Fi para consultar o manifesto.");
    return;
  }
  Serial.println("Consultando o manifesto de versao...");
  HTTPClient http;
  http.begin(URL_MANIFESTO);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError erro = deserializeJson(doc, payload);

    if (erro) {
      Serial.println("Erro: Falha ao interpretar o arquivo version.json.");
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
      String urlFirmware = String(urlBinario);
      Serial.println("Atualizacao encontrada! Preparando o download...");
      Serial.print("URL do Firmware: ");
      Serial.println(urlFirmware);
      http.end();
      aplicarAtualizacao(urlFirmware);
      return;
    } else {
      Serial.println("A versao instalada ja e a mais recente.");
    }
  } else {
    Serial.print("Erro: O manifesto nao pode ser acessado. Codigo HTTP: ");
    Serial.println(httpCode);
  }
  http.end();
}

float calcularMedia() {
  long soma = 0;
  for (uint8_t i = 0; i < QUANTIDADE_LEITURAS; i++) {
    soma += leituras[i];
  }
  return soma / (float)QUANTIDADE_LEITURAS;
}

void ordenarCopia(const int origem[], int destino[]) {
  for (uint8_t i = 0; i < QUANTIDADE_LEITURAS; i++) {
    destino[i] = origem[i];
  }
  for (uint8_t i = 1; i < QUANTIDADE_LEITURAS; i++) {
    int valorAtual = destino[i];
    int j = i - 1;
    while (j >= 0 && destino[j] > valorAtual) {
      destino[j + 1] = destino[j];
      j--;
    }
    destino[j + 1] = valorAtual;
  }
}

int calcularMediana(const int vetorOrdenado[]) {
  return vetorOrdenado[QUANTIDADE_LEITURAS / 2];
}

void imprimirVetor(const char *rotulo, const int vetor[]) {
  Serial.print(rotulo);
  for (uint8_t i = 0; i < QUANTIDADE_LEITURAS; i++) {
    Serial.print(vetor[i]);
    if (i < QUANTIDADE_LEITURAS - 1) Serial.print(" ");
  }
  Serial.println(" cm");
}

void aplicarHisterese(int mediana) {
  if (mediana >= 16) {
    estadoAlerta = true;
  } else if (mediana <= 14) {
    estadoAlerta = false;
  }
  indicarEstado(estadoAlerta);
}

void finalizarSessao() {
  sessaoEmAndamento = false;
  sessoesConcluidas++;

  int ordenadas[QUANTIDADE_LEITURAS];
  ordenarCopia(leituras, ordenadas);
  float media = calcularMedia();
  int mediana = calcularMediana(ordenadas);

  Serial.println();
  Serial.println("--- Resultado da sessao ---");
  imprimirVetor("Ordem original:  ", leituras);
  imprimirVetor("Ordem crescente: ", ordenadas);
  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");
  Serial.print("Mediana da sessao: ");
  Serial.print(mediana);
  Serial.println(" cm");
  
  aplicarHisterese(mediana);
  
  Serial.print("Sessoes concluidas: ");
  Serial.println(sessoesConcluidas);
  Serial.println("Proxima sessao em 48 segundos.");
  Serial.println("---------------------------");
  Serial.println();
}

void realizarLeitura() {
  int alturaCm;
  if (MODO_TESTE_FIXO) {
    alturaCm = leiturasTeste[TESTE_ATIVO % 3][indiceLeitura];
  } else {
    alturaCm = random(10, 21);
  }

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
  indicarEstado(false);
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
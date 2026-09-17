// Firmware de MENTIRA, só para testar o OTA antes de o Firmware 2.0 existir.

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("===========");
  Serial.println("VERSAO 2.0");
  Serial.println("===========");
}

void loop() {
  Serial.println("v2.0 viva: chegou aqui pela rede, sem cabo");
  delay(5000);
}

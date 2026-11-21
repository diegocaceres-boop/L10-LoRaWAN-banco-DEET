// Practica L10 - Parte IV, Paso 13 (Listado L10.5): emisor SF12 en modo radio directo.
// US915 no admite SF11 ni SF12 en la subida LoRaWAN (la trama supera los 400 ms de
// permanencia), asi que se sale de la MAC con "mac pause" y se transmite de banco a banco.
// Cada trama lleva 8 B: "SF12" + un contador de 4 B para que el receptor cuente las que llegan.
// Autor: Diego Caceres
#include <rn2xx3.h>                 // RN2483-Arduino-Library (jpmeijers)

#define LORA_SERIAL loraSerial      // nombre del puerto del slot LORA en este paquete de placas

// Banco emulado: una trama cada 10 s. Con el 1 % de ciclo de trabajo serian unos 100 s.
const unsigned long T_ENVIO = 10000UL;
const uint16_t N_TRAMAS = 60;       // 20 por posicion: P1, P2 y P3

rn2xx3 lora(LORA_SERIAL);

void setup() {
  Serial.begin(115200);
  LORA_SERIAL.begin(57600);
  delay(1000);
  lora.autobaud();
  Serial.println(lora.hweui());
  Serial.println(lora.sysver());

  Serial.println("mac pause -> " + lora.sendRawCommand("mac pause"));   // suspende la MAC LoRaWAN
  lora.sendRawCommand("radio set mod lora");
  lora.sendRawCommand("radio set freq 903900000");   // canal 8, sub-banda 2
  lora.sendRawCommand("radio set bw 125");
  lora.sendRawCommand("radio set cr 4/5");
  Serial.println("radio set sf sf12 -> " + lora.sendRawCommand("radio set sf sf12"));
}

void loop() {
  static uint16_t n_tx = 0;
  static unsigned long ultimo = 0;
  if (n_tx >= N_TRAMAS) return;
  if (n_tx > 0 && millis() - ultimo < T_ENVIO) return;
  ultimo = millis();
  n_tx++;

  char hex[17];
  snprintf(hex, sizeof(hex), "53463132%08X", (unsigned)n_tx);   // "SF12" + contador

  while (LORA_SERIAL.available()) LORA_SERIAL.read();
  unsigned long t0 = millis();                   // se cronometra hasta radio_tx_ok
  LORA_SERIAL.print("radio tx ");
  LORA_SERIAL.println(hex);
  LORA_SERIAL.setTimeout(5000);
  String r1 = LORA_SERIAL.readStringUntil('\n'); r1.trim();      // "ok"
  String r2 = LORA_SERIAL.readStringUntil('\n'); r2.trim();      // "radio_tx_ok"
  unsigned long dt = millis() - t0;
  LORA_SERIAL.setTimeout(2000);

  if (r1 == "ok" && r2 == "radio_tx_ok")
    Serial.printf("TX #%u SF12 8 B: %lu ms hasta radio_tx_ok\n", n_tx, dt);
  else
    Serial.printf("TX #%u fallida: %s / %s\n", n_tx, r1.c_str(), r2.c_str());
  if (n_tx % 20 == 0) Serial.println("--- 20 tramas: cambia de posicion (P1/P2/P3) ---");

  if (n_tx == N_TRAMAS)                          // IMPRESCINDIBLE: devolver el modulo a LoRaWAN
    Serial.println("Fin. mac resume -> " + lora.sendRawCommand("mac resume"));
}

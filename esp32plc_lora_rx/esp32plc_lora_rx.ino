// Practica L10 - Parte IV, Paso 13 (Listado L10.5): receptor en modo radio directo, en el
// ESP32 PLC del tablero 2 (con su antena puesta). Escucha sin limite en el canal 8 con SF12
// y, por cada trama, imprime su contador, el RSSI y la SNR medidos por el RN2903.
// Autor: Diego Caceres
#include <rn2xx3.h>                 // RN2483-Arduino-Library (jpmeijers)

#define LORA_SERIAL loraSerial      // nombre del puerto del slot LORA en este paquete de placas

rn2xx3 lora(LORA_SERIAL);

void setup() {
  Serial.begin(115200);
  LORA_SERIAL.begin(57600);
  delay(1000);
  lora.autobaud();
  Serial.println(lora.hweui());
  Serial.println(lora.sysver());

  Serial.println("mac pause -> " + lora.sendRawCommand("mac pause"));
  lora.sendRawCommand("radio set mod lora");
  lora.sendRawCommand("radio set freq 903900000");   // mismos parametros que el emisor
  lora.sendRawCommand("radio set bw 125");
  lora.sendRawCommand("radio set cr 4/5");
  lora.sendRawCommand("radio set sf sf12");
  lora.sendRawCommand("radio set wdt 0");            // escucha indefinida (de fabrica, 15 s)
  Serial.println("Receptor SF12 listo en 903,9 MHz");
}

void loop() {
  static uint16_t n_rx = 0;
  String r = lora.sendRawCommand("radio rx 0");     // "ok": empieza a escuchar
  if (r != "ok") { Serial.println("radio rx: " + r); delay(1000); return; }

  LORA_SERIAL.setTimeout(600000);                    // espera la trama (hasta 10 min)
  String d = LORA_SERIAL.readStringUntil('\n'); d.trim();
  LORA_SERIAL.setTimeout(2000);
  if (!d.startsWith("radio_rx")) { Serial.println("sin trama: " + d); return; }

  String datos = d.substring(d.lastIndexOf(' ') + 1);
  unsigned long num = strtoul(datos.substring(8).c_str(), nullptr, 16);   // contador del emisor
  String rssi = lora.sendRawCommand("radio get rssi");
  String snr = lora.sendRawCommand("radio get snr");
  n_rx++;
  Serial.printf("RX trama #%lu (recibidas %u): RSSI %s dBm, SNR %s dB, datos %s\n",
                num, n_rx, rssi.c_str(), snr.c_str(), datos.c_str());
}

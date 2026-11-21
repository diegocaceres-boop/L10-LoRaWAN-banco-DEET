// Practica L10 - Partes I a III (Listados L10.1 y L10.3): el ESP32 PLC 14 como pasarela entre
// el Controllino (esclavo Modbus 1 por RS-485) y LoRaWAN (nodo de clase A, US915 FSB 2).
// Autor: Diego Caceres
#include <ModbusRTU.h>              // modbus-esp8266 (maestro, como en L6)
#include <RTClib.h>                 // DS3231 por I2C
#include <rn2xx3.h>                 // RN2483-Arduino-Library (jpmeijers)

#define LORA_SERIAL loraSerial      // nombre del puerto del slot LORA en este paquete de placas
#define SLAVE_ID 1
#define N_REGS 10                   // [0..7] canonicos + [8] A1 + [9] estado

// Identidades OTAA de ChirpStack. NO subir al repositorio con las claves reales.
const char* JOIN_EUI = "0000000000000000";              // 16 caracteres hex
const char* APP_KEY  = "00000000000000000000000000000000";               // 32 caracteres hex

// Parte IV: DR0 = SF10, DR1 = SF9, DR3 = SF7. Intervalo minimo al 1 %: SF7 5,7 s, SF9 20,6 s.
const int DR = 1;                        // SF9
const unsigned long T_ENVIO = 21000UL;   // 21 s
//const int DR = 3;                       // SF7
//const unsigned long T_ENVIO = 6000UL;   // 6 s
//const int DR = 0;
//const unsigned long T_ENVIO = 600000UL;                      // 1 envio cada 10 min

ModbusRTU mb;
RTC_DS3231 rtc;
rn2xx3 lora(LORA_SERIAL);
uint16_t reg[N_REGS];
uint8_t trama[9];
volatile bool listo = false;

bool cbLectura(Modbus::ResultCode ev, uint16_t, void*) {
  listo = (ev == Modbus::EX_SUCCESS);
  if (!listo) Serial.printf("Modbus error 0x%02X\n", ev);
  return true;
}

uint16_t aDecimas(uint16_t c) { return (uint32_t)c * 1000UL / 1023UL; }

// Fig. L10.4: byte 0 = VER (b7-b4) = 1 | A_T (b3) | A_N (b2); bytes 1-2 TEMP y 3-4 NIV
// (decimas, big-endian); bytes 5-8 marca de tiempo Unix del DS3231.
void empaquetar() {
  uint16_t temp = aDecimas(reg[0]);
  uint16_t niv  = aDecimas(reg[8]);
  uint8_t aT = reg[9] & 0x01;               // b0 del registro de estado del esclavo
  uint8_t aN = (reg[9] >> 1) & 0x01;        // b1
  uint32_t ts = rtc.now().unixtime();

  trama[0] = (1 << 4) | (aT << 3) | (aN << 2);
  trama[1] = temp >> 8;  trama[2] = temp & 0xFF;
  trama[3] = niv >> 8;   trama[4] = niv & 0xFF;
  trama[5] = ts >> 24;   trama[6] = ts >> 16;  trama[7] = ts >> 8;  trama[8] = ts;

  Serial.printf("TEMP %u  NIV %u  ALM_T %u  ALM_N %u  TS %lu\n", temp, niv, aT, aN, (unsigned long)ts);
  Serial.print("Trama:");
  for (int i = 0; i < 9; i++) Serial.printf(" %02X", trama[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  RS485.begin(19200, SERIAL_8N1);   // valores canonicos del banco
  mb.begin(&RS485); mb.master();
  rtc.begin();
  LORA_SERIAL.begin(57600);
  delay(1000);
  lora.autobaud();
  Serial.println(lora.hweui());     // DevEUI de fabrica: el que se registra
  Serial.println(lora.sysver());

  lora.setFrequencyPlan(TTN_US);    // solo canales 8-15 y 65: la sub-banda 2 de la pasarela
  while (!lora.initOTAA(JOIN_EUI, APP_KEY)) {
    Serial.println("Join rechazado; reintento en 30 s");
    delay(30000);
  }
  Serial.println("Union completada: sesion activa");
  lora.setDR(DR);
}

void loop() {
  if (!mb.slave()) {                // bus libre: nueva consulta
    listo = false;
    mb.readHreg(SLAVE_ID, 0, reg, N_REGS, cbLectura);       // FC3, base 0
  }
  mb.task(); yield();

  static bool primera = true;       // el primer ascendente sale apenas hay dato del PLC
  static uint32_t ultimo = 0;
  static uint16_t n_tx = 0;         // contador para las rafagas de 20 de la Parte IV
  if (listo && (primera || millis() - ultimo >= T_ENVIO)) {
    primera = false;
    ultimo = millis();
    empaquetar();
    TX_RETURN_TYPE r = lora.txBytes(trama, sizeof(trama));  // 9 B de FRMPayload
    n_tx++;
    Serial.printf("TX #%u %s\n", n_tx, r == TX_FAIL ? "fallida" : "enviada");
    if (n_tx % 20 == 0) Serial.println("--- 20 tramas: cambia de posicion (P1/P2/P3) ---");
  }
}
